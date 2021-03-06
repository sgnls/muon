'use strict'

const Buffer = require('buffer').Buffer
const v8Util = requireNative('atom').GetBinding().v8
const ipcRenderer = require('ipc_utils')
const isPromise = require('is-promise').isPromise
const CallbacksRegistry = require('callbacks-registry').CallbacksRegistry
const GuestViewInternal = require('guest-view-internal').GuestViewInternal

const callbacksRegistry = new CallbacksRegistry()

const remoteObjectCache = v8Util.createIDWeakMap()

// Convert the arguments object into an array of meta data.
const wrapArgs = function (args, visited) {
  if (visited == null) {
    visited = new Set()
  }

  const valueToMeta = function (value) {
    // Check for circular reference.
    if (visited.has(value)) {
      return {
        type: 'value',
        value: null
      }
    }

    if (Array.isArray(value)) {
      visited.add(value)
      let meta = {
        type: 'array',
        value: wrapArgs(value, visited)
      }
      visited.delete(value)
      return meta
    } else if (ArrayBuffer.isView(value)) {
      return {
        type: 'buffer',
        value: Buffer.from(value)
      }
    } else if (value instanceof Date) {
      return {
        type: 'date',
        value: value.getTime()
      }
    } else if ((value != null) && typeof value === 'object') {
      if (isPromise(value)) {
        return {
          type: 'promise',
          then: valueToMeta(function (onFulfilled, onRejected) {
            value.then(onFulfilled, onRejected)
          })
        }
      } else if (privates(value).atomId) {
        return {
          type: 'remote-object',
          id: privates(value).atomId
        }
      }

      let meta = {
        type: 'object',
        name: value.constructor != null ? value.constructor.name : '',
        members: []
      }
      visited.add(value)
      for (let prop in value) {
        meta.members.push({
          name: prop,
          value: valueToMeta(value[prop])
        })
      }
      visited.delete(value)
      return meta
    } else if (typeof value === 'function' && privates(value).returnValue) {
      return {
        type: 'function-with-return-value',
        value: valueToMeta(value())
      }
    } else if (typeof value === 'function') {
      return {
        type: 'function',
        id: callbacksRegistry.add(value),
        location: privates(value).location
      }
    } else {
      return {
        type: 'value',
        value: value
      }
    }
  }
  return Array.prototype.slice.call(args).map(valueToMeta)
}

// Populate object's members from descriptors.
// The |ref| will be kept referenced by |members|.
// This matches |getObjectMemebers| in rpc-server.
const setObjectMembers = function (ref, object, metaId, members) {
  for (let member of members) {
    if (object.hasOwnProperty(member.name)) continue

    let descriptor = { enumerable: member.enumerable }
    if (member.type === 'method') {
      const remoteMemberFunction = function () {
        if (this && this.constructor === remoteMemberFunction) {
          // Constructor call.
          let ret = ipcRenderer.sendSync('ELECTRON_BROWSER_MEMBER_CONSTRUCTOR', metaId, member.name, wrapArgs(arguments))
          return metaToValue(ret)
        } else {
          // Call member function.
          let ret = ipcRenderer.sendSync('ELECTRON_BROWSER_MEMBER_CALL', metaId, member.name, wrapArgs(arguments))
          return metaToValue(ret)
        }
      }

      let descriptorFunction = proxyFunctionProperties(remoteMemberFunction, metaId, member.name)

      descriptor.get = function () {
        descriptorFunction.ref = ref  // The member should reference its object.
        return descriptorFunction
      }
      // Enable monkey-patch the method
      descriptor.set = function (value) {
        descriptorFunction = value
        return value
      }
      descriptor.configurable = true
    } else if (member.type === 'get') {
      descriptor.get = function () {
        return metaToValue(ipcRenderer.sendSync('ELECTRON_BROWSER_MEMBER_GET', metaId, member.name))
      }

      // Only set setter when it is writable.
      if (member.writable) {
        descriptor.set = function (value) {
          ipcRenderer.sendSync('ELECTRON_BROWSER_MEMBER_SET', metaId, member.name, value)
          return value
        }
      }
    }

    Object.defineProperty(object, member.name, descriptor)
  }
}

// Populate object's prototype from descriptor.
// This matches |getObjectPrototype| in rpc-server.
const setObjectPrototype = function (ref, object, metaId, descriptor) {
  if (descriptor === null) return
  let proto = {}
  setObjectMembers(ref, proto, metaId, descriptor.members)
  setObjectPrototype(ref, proto, metaId, descriptor.proto)
  Object.setPrototypeOf(object, proto)
}

// Wrap function in Proxy for accessing remote properties
const proxyFunctionProperties = function (remoteMemberFunction, metaId, name) {
  let loaded = false

  // Lazily load function properties
  const loadRemoteProperties = () => {
    if (loaded) return
    loaded = true
    const meta = ipcRenderer.sendSync('ELECTRON_BROWSER_MEMBER_GET', metaId, name)
    if (Array.isArray(meta.members)) {
      setObjectMembers(remoteMemberFunction, remoteMemberFunction, meta.id, meta.members)
    }
  }

  return new Proxy(remoteMemberFunction, {
    set: (target, property, value, receiver) => {
      if (property !== 'ref') loadRemoteProperties()
      target[property] = value
      return true
    },
    get: (target, property, receiver) => {
      if (!target.hasOwnProperty(property)) loadRemoteProperties()
      return target[property]
    },
    ownKeys: (target) => {
      loadRemoteProperties()
      return Object.getOwnPropertyNames(target)
    },
    getOwnPropertyDescriptor: (target, property) => {
      let descriptor = Object.getOwnPropertyDescriptor(target, property)
      if (descriptor != null) return descriptor
      loadRemoteProperties()
      return Object.getOwnPropertyDescriptor(target, property)
    }
  })
}

// Convert meta data from browser into real value.
const metaToValue = function (meta) {
  var el, i, len, ref1, results, ret
  switch (meta.type) {
    case 'value':
      return meta.value
    case 'array':
      ref1 = meta.members
      results = []
      for (i = 0, len = ref1.length; i < len; i++) {
        el = ref1[i]
        results.push(metaToValue(el))
      }
      return results
    case 'buffer':
      return Buffer.from(meta.value)
    case 'promise':
      return Promise.resolve({
        then: metaToValue(meta.then)
      })
    case 'error':
      return metaToPlainObject(meta)
    case 'date':
      return new Date(meta.value)
    case 'exception':
      throw new Error(meta.message + '\n' + meta.stack)
    default:
      if (remoteObjectCache.has(meta.id)) return remoteObjectCache.get(meta.id)

      if (meta.type === 'function') {
        // A shadow class to represent the remote function object.
        let remoteFunction = function () {
          if (this && this.constructor === remoteFunction) {
            // Constructor call.
            let obj = ipcRenderer.sendSync('ELECTRON_BROWSER_CONSTRUCTOR', meta.id, wrapArgs(arguments))
            // Returning object in constructor will replace constructed object
            // with the returned object.
            // http://stackoverflow.com/questions/1978049/what-values-can-a-constructor-return-to-avoid-returning-this
            return metaToValue(obj)
          } else {
            // Function call.
            let obj = ipcRenderer.sendSync('ELECTRON_BROWSER_FUNCTION_CALL', meta.id, wrapArgs(arguments))
            return metaToValue(obj)
          }
        }
        ret = remoteFunction
      } else {
        ret = {}
      }

      // Populate delegate members.
      setObjectMembers(ret, ret, meta.id, meta.members)
      // Populate delegate prototype.
      setObjectPrototype(ret, ret, meta.id, meta.proto)

      // Set constructor.name to object's name.
      Object.defineProperty(ret.constructor, 'name', { value: meta.name })

      // Track delegate object's life time, and tell the browser to clean up
      // when the object is GCed.
      v8Util.setRemoteObjectFreer(ret, meta.id)

      // Remember object's id.
      privates(ret).atomId = meta.id
      remoteObjectCache.set(meta.id, ret)
      return ret
  }
}

// Construct a plain object from the meta.
const metaToPlainObject = function (meta) {
  var i, len, obj, ref1
  obj = (function () {
    switch (meta.type) {
      case 'error':
        return new Error()
      default:
        return {}
    }
  })()
  ref1 = meta.members
  for (i = 0, len = ref1.length; i < len; i++) {
    let {name, value} = ref1[i]
    obj[name] = value
  }
  return obj
}

// Browser calls a callback in renderer.
ipcRenderer.on('ELECTRON_RENDERER_CALLBACK', function (event, id, args) {
  callbacksRegistry.apply(id, metaToValue(args))
})

// // A callback in browser is released.
ipcRenderer.on('ELECTRON_RENDERER_RELEASE_CALLBACK', function (event, id) {
  callbacksRegistry.remove(id)
})

var binding = {}

binding.require = function (module) {
  return metaToValue(ipcRenderer.sendSync('ELECTRON_BROWSER_REQUIRE', module))
}

// Alias to remote.require('electron').xxx.
binding.getBuiltin = function (module) {
  return metaToValue(ipcRenderer.sendSync('ELECTRON_BROWSER_GET_BUILTIN', module))
}

// Get current BrowserWindow.
binding.getCurrentWindow = function () {
  return metaToValue(ipcRenderer.sendSync('ELECTRON_BROWSER_CURRENT_WINDOW'))
}

// Get current WebContents object.
binding.getCurrentWebContents = function () {
  return metaToValue(ipcRenderer.sendSync('ELECTRON_BROWSER_CURRENT_WEB_CONTENTS'))
}

binding.getWebContents = function (tabId, cb) {
  const responseId = ipcRenderer.guid()
  ipcRenderer.on('ELECTRON_BROWSER_GET_WEB_CONTENTS_RESPONSE_' + responseId, (evt, res) => {
    cb(metaToValue(res))
  })
  ipcRenderer.send('ELECTRON_BROWSER_GET_WEB_CONTENTS', tabId, responseId)
}

binding.callAsyncWebContentsFunction = function (tabId, name, ...args) {
  ipcRenderer.send('ELECTRON_BROWSER_ASYNC_MEMBER_CALL', tabId, name, wrapArgs(...args))
}

binding.registerEvents = function (tabID, listener) {
  GuestViewInternal.addListener(tabID, listener)
}

binding.registerAllWindowTabEvents = function (listener) {
  GuestViewInternal.addListener('window-tab-event', listener)
}

binding.unregisterEvents = function (tabID, listener) {
  GuestViewInternal.removeListener(tabID, listener)
}

const deprecatedRemoteAPIs = ['Menu', 'shell', 'screen', 'clipboard', 'session', 'BrowserWindow']
for (var i = 0, len = deprecatedRemoteAPIs.length; i < len; i++) {
  const name = deprecatedRemoteAPIs[i]
  Object.defineProperty(binding, name, {
    get: function () {
      return binding.getBuiltin(name)
    }
  })
}

exports.$set('registerEvents', binding.registerEvents)
exports.$set('unregisterEvents', binding.unregisterEvents)
exports.$set('callAsyncWebContentsFunction', binding.callAsyncWebContentsFunction)
exports.$set('getWebContents', binding.getWebContents)
exports.$set('getCurrentWebContents', binding.getCurrentWebContents)
exports.$set('binding', binding)
