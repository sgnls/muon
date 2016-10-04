// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/profile_writer.h"

#include <stddef.h>

#include <map>
#include <set>
#include <string>

#include "atom/browser/api/atom_api_importer.h"
#include "atom/common/importer/imported_cookie_entry.h"
#include "atom/common/native_mate_converters/string16_converter.h"
#include "atom/common/native_mate_converters/value_converter.h"
#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
// #include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/chrome_notification_types.h"
// #include "chrome/browser/favicon/favicon_service_factory.h"
// #include "chrome/browser/history/history_service_factory.h"
// #include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
// #include "chrome/browser/search_engines/template_url_service_factory.h"
// #include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/common/password_form.h"
// #include
// "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/bookmarks/browser/bookmark_model.h"
// #include "components/bookmarks/common/bookmark_pref_names.h"
// #include "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/history_service.h"
// #include "components/password_manager/core/browser/password_store.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"

#if defined(OS_WIN)
// #include "chrome/browser/web_data_service_factory.h"
// #include "components/password_manager/core/browser/webdata/\
// password_web_data_service_win.h"
#endif
/*
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

// Generates a unique folder name. If |folder_name| is not unique, then this
// repeatedly tests for '|folder_name| + (i)' until a unique name is found.
base::string16 GenerateUniqueFolderName(BookmarkModel* model,
                                        const base::string16& folder_name) {
  // Build a set containing the bookmark bar folder names.
  std::set<base::string16> existing_folder_names;
  const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
  for (int i = 0; i < bookmark_bar->child_count(); ++i) {
    const BookmarkNode* node = bookmark_bar->GetChild(i);
    if (node->is_folder())
      existing_folder_names.insert(node->GetTitle());
  }

  // If the given name is unique, use it.
  if (existing_folder_names.find(folder_name) == existing_folder_names.end())
    return folder_name;

  // Otherwise iterate until we find a unique name.
  for (size_t i = 1; i <= existing_folder_names.size(); ++i) {
    base::string16 name = folder_name + base::ASCIIToUTF16(" (") +
        base::SizeTToString16(i) + base::ASCIIToUTF16(")");
    if (existing_folder_names.find(name) == existing_folder_names.end())
      return name;
  }

  NOTREACHED();
  return folder_name;
}

// Shows the bookmarks toolbar.
void ShowBookmarkBar(Profile* profile) {
  //profile->GetPrefs()->SetBoolean(bookmarks::prefs::kShowBookmarkBar, true);
}

}  // namespace
*/

ProfileWriter::ProfileWriter(Profile* profile) : profile_(profile),
                                                 importer_(nullptr) {}

bool ProfileWriter::BookmarkModelIsLoaded() const {
  // return BookmarkModelFactory::GetForProfile(profile_)->loaded();
  return true;
}

bool ProfileWriter::TemplateURLServiceIsLoaded() const {
  // return TemplateURLServiceFactory::GetForProfile(profile_)->loaded();
  return true;
}

void ProfileWriter::AddPasswordForm(const autofill::PasswordForm& form) {
  /*
  PasswordStoreFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS)->AddLogin(form);
      */
  if (importer_) {
    base::DictionaryValue imported_form;
    imported_form.SetString("signon_realm", form.signon_realm);
    imported_form.SetString("username_value", form.username_value);
    imported_form.SetString("password_value", form.password_value);

    importer_->Emit("add-password-form", imported_form);
  }
}

#if defined(OS_WIN)
void ProfileWriter::AddIE7PasswordInfo(const IE7PasswordInfo& info) {
  /*
  WebDataServiceFactory::GetPasswordWebDataForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS)->AddIE7Login(info);
      */
}
#endif

void ProfileWriter::AddHistoryPage(const history::URLRows& page,
                                   history::VisitSource visit_source) {
  /*
  HistoryServiceFactory::GetForProfile(profile_,
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->AddPagesWithDetails(page, visit_source);
      */
  if (importer_) {
    base::ListValue history_list;
    for (const history::URLRow& row : page) {
      base::DictionaryValue* history = new base::DictionaryValue();
      history->SetString("title", row.title());
      history->SetString("url", row.url().possibly_invalid_spec());
      history->SetInteger("visit_count", row.visit_count());
      history->SetInteger("last_visit", row.last_visit().ToDoubleT());
      history_list.Append(history);
    }
    importer_->Emit("add-history-page", history_list,
                    (unsigned int) visit_source);
  }
}

void ProfileWriter::AddHomepage(const GURL& home_page) {
  /*
  DCHECK(profile_);
  PrefService* prefs = profile_->GetPrefs();
  // NOTE: We set the kHomePage value, but keep the NewTab page as the homepage.
  const PrefService::Preference* pref = prefs->FindPreference(prefs::kHomePage);
  if (pref && !pref->IsManaged()) {
    prefs->SetString(prefs::kHomePage, home_page.spec());
  }
  */
  if (importer_) {
    importer_->Emit("add-homepage", home_page.possibly_invalid_spec());
  }
}

void ProfileWriter::AddBookmarks(
    const std::vector<ImportedBookmarkEntry>& bookmarks,
    const base::string16& top_level_folder_name) {
  /*
  if (bookmarks.empty())
    return;

  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile_);
  DCHECK(model->loaded());

  // If the bookmark bar is currently empty, we should import directly to it.
  // Otherwise, we should import everything to a subfolder.
  const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
  bool import_to_top_level = bookmark_bar->empty();

  // Reorder bookmarks so that the toolbar entries come first.
  std::vector<ImportedBookmarkEntry> toolbar_bookmarks;
  std::vector<ImportedBookmarkEntry> reordered_bookmarks;
  for (std::vector<ImportedBookmarkEntry>::const_iterator it =
           bookmarks.begin();
       it != bookmarks.end(); ++it) {
    if (it->in_toolbar)
      toolbar_bookmarks.push_back(*it);
    else
      reordered_bookmarks.push_back(*it);
  }
  reordered_bookmarks.insert(reordered_bookmarks.begin(),
                             toolbar_bookmarks.begin(),
                             toolbar_bookmarks.end());

  // If the user currently has no bookmarks in the bookmark bar, make sure that
  // at least some of the imported bookmarks end up there.  Otherwise, we'll end
  // up with just a single folder containing the imported bookmarks, which makes
  // for unnecessary nesting.
  bool add_all_to_top_level = import_to_top_level && toolbar_bookmarks.empty();

  model->BeginExtensiveChanges();

  std::set<const BookmarkNode*> folders_added_to;
  const BookmarkNode* top_level_folder = NULL;
  for (std::vector<ImportedBookmarkEntry>::const_iterator bookmark =
           reordered_bookmarks.begin();
       bookmark != reordered_bookmarks.end(); ++bookmark) {
    // Disregard any bookmarks with invalid urls.
    if (!bookmark->is_folder && !bookmark->url.is_valid())
      continue;

    const BookmarkNode* parent = NULL;
    if (import_to_top_level && (add_all_to_top_level || bookmark->in_toolbar)) {
      // Add directly to the bookmarks bar.
      parent = bookmark_bar;
    } else {
      // Add to a folder that will contain all the imported bookmarks not added
      // to the bar.  The first time we do so, create the folder.
      if (!top_level_folder) {
        base::string16 name =
            GenerateUniqueFolderName(model,top_level_folder_name);
        top_level_folder = model->AddFolder(bookmark_bar,
                                            bookmark_bar->child_count(),
                                            name);
      }
      parent = top_level_folder;
    }

    // Ensure any enclosing folders are present in the model.  The bookmark's
    // enclosing folder structure should be
    //   path[0] > path[1] > ... > path[size() - 1]
    for (std::vector<base::string16>::const_iterator folder_name =
             bookmark->path.begin();
         folder_name != bookmark->path.end(); ++folder_name) {
      if (bookmark->in_toolbar && parent == bookmark_bar &&
          folder_name == bookmark->path.begin()) {
        // If we're importing directly to the bookmarks bar, skip over the
        // folder named "Bookmarks Toolbar" (or any non-Firefox equivalent).
        continue;
      }

      const BookmarkNode* child = NULL;
      for (int index = 0; index < parent->child_count(); ++index) {
        const BookmarkNode* node = parent->GetChild(index);
        if (node->is_folder() && node->GetTitle() == *folder_name) {
          child = node;
          break;
        }
      }
      if (!child)
        child = model->AddFolder(parent, parent->child_count(), *folder_name);
      parent = child;
    }

    folders_added_to.insert(parent);
    if (bookmark->is_folder) {
      model->AddFolder(parent, parent->child_count(), bookmark->title);
    } else {
      model->AddURLWithCreationTimeAndMetaInfo(parent,
                                               parent->child_count(),
                                               bookmark->title,
                                               bookmark->url,
                                               bookmark->creation_time,
                                               NULL);
    }
  }

  // In order to keep the imported-to folders from appearing in the 'recently
  // added to' combobox, reset their modified times.
  for (std::set<const BookmarkNode*>::const_iterator i =
           folders_added_to.begin();
       i != folders_added_to.end(); ++i) {
    model->ResetDateFolderModified(*i);
  }

  model->EndExtensiveChanges();

  // If the user was previously using a toolbar, we should show the bar.
  if (import_to_top_level && !add_all_to_top_level)
    ShowBookmarkBar(profile_);
  */
  if (importer_) {
    base::ListValue imported_bookmarks;
    for (const ImportedBookmarkEntry& bookmark : bookmarks) {
      base::DictionaryValue* imported_bookmark = new base::DictionaryValue();
      imported_bookmark->SetBoolean("in_toolbar", bookmark.in_toolbar);
      imported_bookmark->SetBoolean("is_folder", bookmark.is_folder);
      imported_bookmark->SetString("url", bookmark.url.possibly_invalid_spec());
      imported_bookmark->SetString("title", bookmark.title);
      imported_bookmark->SetInteger("creation_time",
                                    bookmark.creation_time.ToDoubleT());
      base::ListValue* paths = new base::ListValue();
      for (const base::string16& path : bookmark.path) {
        paths->AppendString(path);
      }
      imported_bookmark->Set("path", paths);
      imported_bookmarks.Append(imported_bookmark);
    }
    importer_->Emit("add-bookmarks", imported_bookmarks, top_level_folder_name);
  }
}

void ProfileWriter::AddFavicons(
    const favicon_base::FaviconUsageDataList& favicons) {
  /*
  FaviconServiceFactory::GetForProfile(profile_,
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->SetImportedFavicons(favicons);
      */
  if (importer_) {
    base::ListValue imported_favicons;
    for (const favicon_base::FaviconUsageData& favicon : favicons) {
      base::DictionaryValue* imported_favicon = new base::DictionaryValue();
      imported_favicon->SetString("favicon_url",
                                  favicon.favicon_url.possibly_invalid_spec());
      std::string data_url;
      data_url.insert(data_url.end(), favicon.png_data.begin(),
                      favicon.png_data.end());
      base::Base64Encode(data_url, &data_url);
      data_url.insert(0, "data:image/png;base64,");
      imported_favicon->SetString("png_data", data_url);
      std::set<GURL>::iterator it;
      base::ListValue* urls = new base::ListValue();
      for (it = favicon.urls.begin(); it != favicon.urls.end(); ++it) {
        urls->AppendString(it->possibly_invalid_spec());
      }
      imported_favicon->Set("urls", urls);
      imported_favicons.Append(imported_favicon);
    }
    importer_->Emit("add-favicons", imported_favicons);
  }
}

/*
typedef std::map<std::string, TemplateURL*> HostPathMap;

// Returns the key for the map built by BuildHostPathMap. If url_string is not
// a valid URL, an empty string is returned, otherwise host+path is returned.
static std::string HostPathKeyForURL(const GURL& url) {
  return url.is_valid() ? url.host() + url.path() : std::string();
}

// Builds the key to use in HostPathMap for the specified TemplateURL. Returns
// an empty string if a host+path can't be generated for the TemplateURL.
// If an empty string is returned, the TemplateURL should not be added to
// HostPathMap.
//
// If |try_url_if_invalid| is true, and |t_url| isn't valid, a string is built
// from the raw TemplateURL string. Use a value of true for |try_url_if_invalid|
// when checking imported URLs as the imported URL may not be valid yet may
// match the host+path of one of the default URLs. This is used to catch the
// case of IE using an invalid OSDD URL for Live Search, yet the host+path
// matches our prepopulate data. IE's URL for Live Search is something like
// 'http://...{Language}...'. As {Language} is not a valid OSDD parameter value
// the TemplateURL is invalid.
static std::string BuildHostPathKey(const TemplateURL* t_url,
                                    const SearchTermsData& search_terms_data,
                                    bool try_url_if_invalid) {
  if (try_url_if_invalid && !t_url->url_ref().IsValid(search_terms_data))
    return HostPathKeyForURL(GURL(t_url->url()));

  if (t_url->url_ref().SupportsReplacement(search_terms_data)) {
    return HostPathKeyForURL(GURL(
        t_url->url_ref().ReplaceSearchTerms(
            TemplateURLRef::SearchTermsArgs(base::ASCIIToUTF16("x")),
            search_terms_data)));
  }
  return std::string();
}

// Builds a set that contains an entry of the host+path for each TemplateURL in
// the TemplateURLService that has a valid search url.
static void BuildHostPathMap(TemplateURLService* model,
                             HostPathMap* host_path_map) {
  TemplateURLService::TemplateURLVector template_urls =
      model->GetTemplateURLs();
  for (size_t i = 0; i < template_urls.size(); ++i) {
    const std::string host_path = BuildHostPathKey(
        template_urls[i], model->search_terms_data(), false);
    if (!host_path.empty()) {
      const TemplateURL* existing_turl = (*host_path_map)[host_path];
      if (!existing_turl ||
          (template_urls[i]->show_in_default_list() &&
           !existing_turl->show_in_default_list())) {
        // If there are multiple TemplateURLs with the same host+path, favor
        // those shown in the default list.  If there are multiple potential
        // defaults, favor the first one, which should be the more commonly used
        // one.
        (*host_path_map)[host_path] = template_urls[i];
      }
    }  // else case, TemplateURL doesn't have a search url, doesn't support
       // replacement, or doesn't have valid GURL. Ignore it.
  }
}
*/

/*
void ProfileWriter::AddKeywords(ScopedVector<TemplateURL> template_urls,
                                bool unique_on_host_and_path) {
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(profile_);
  HostPathMap host_path_map;
  if (unique_on_host_and_path)
    BuildHostPathMap(model, &host_path_map);

  for (ScopedVector<TemplateURL>::iterator i = template_urls.begin();
       i != template_urls.end(); ++i) {
    // TemplateURLService requires keywords to be unique. If there is already a
    // TemplateURL with this keyword, don't import it again.
    if (model->GetTemplateURLForKeyword((*i)->keyword()) != NULL)
      continue;

    // For search engines if there is already a keyword with the same
    // host+path, we don't import it. This is done to avoid both duplicate
    // search providers (such as two Googles, or two Yahoos) as well as making
    // sure the search engines we provide aren't replaced by those from the
    // imported browser.
    if (unique_on_host_and_path &&
        (host_path_map.find(BuildHostPathKey(
            *i, model->search_terms_data(), true)) != host_path_map.end()))
      continue;

    // Only add valid TemplateURLs to the model.
    if ((*i)->url_ref().IsValid(model->search_terms_data())) {
      model->Add(*i);  // Takes ownership.
      *i = NULL;  // Prevent the vector from deleting *i later.
    }
  }
  if (importer_) {
    base::ListValue imported_template_urls;
    for (ScopedVector<TemplateURL>::iterator i = template_urls.begin();
         i != template_urls.end(); ++i) {
      base::DictionaryValue* imported_template_url =
        new base::DictionaryValue();
      imported_template_url->SetString("short_name", (*i)->short_name());
      imported_template_url->SetString("keyword", (*i)->keyword());
      imported_template_url->SetString("url", (*i)->url());
      imported_template_url->SetString("favicon_url",
        (*i)->favicon_url().possibly_invalid_spec());
      imported_template_url->SetString("suggestions_url",
                                       (*i)->suggestions_url());

      imported_template_urls.Append(imported_template_url);
    }
    importer_->Emit("add-keywords", imported_template_urls,
                    unique_on_host_and_path);
  }
}
*/

void ProfileWriter::AddAutofillFormDataEntries(
    const std::vector<autofill::AutofillEntry>& autofill_entries) {
  /*
  scoped_refptr<autofill::AutofillWebDataService> web_data_service =
      WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (web_data_service.get())
    web_data_service->UpdateAutofillEntries(autofill_entries);
    */
  if (importer_) {
    base::ListValue imported_autofill_entries;
    for (const autofill::AutofillEntry& autofill_entry : autofill_entries) {
      base::DictionaryValue* imported_autofill_entry =
        new base::DictionaryValue();
      imported_autofill_entry->SetString("name", autofill_entry.key().name());
      imported_autofill_entry->SetString("value", autofill_entry.key().value());
      imported_autofill_entries.Append(imported_autofill_entry);
    }
    importer_->Emit("add-autofill-form-data-entries",
                    imported_autofill_entries);
  }
}

void ProfileWriter::AddCookies(
    const std::vector<ImportedCookieEntry>& cookies) {
  if (importer_) {
    base::ListValue imported_cookies;
    for (const ImportedCookieEntry& cookie_entry : cookies) {
      base::DictionaryValue* cookie = new base::DictionaryValue();
      base::string16 url;
      if (cookie_entry.httponly) {
        url.append(base::UTF8ToUTF16("http://"));
        url.append(cookie_entry.host);
      } else {
        url.append(base::UTF8ToUTF16("https://"));
        url.append(cookie_entry.host);
      }
      cookie->SetString("url", url);
      cookie->SetString("domain", cookie_entry.domain);
      cookie->SetString("name", cookie_entry.name);
      cookie->SetString("value", cookie_entry.value);
      cookie->SetString("path", cookie_entry.path);
      cookie->SetInteger("expiry_date", cookie_entry.expiry_date.ToDoubleT());
      cookie->SetBoolean("secure", cookie_entry.secure);
      cookie->SetBoolean("httponly", cookie_entry.httponly);
      imported_cookies.Append(cookie);
    }
    importer_->Emit("add-cookies", imported_cookies);
  }
}

void ProfileWriter::Initialize(atom::api::Importer* importer) {
  importer_ = importer;
}

void ProfileWriter::ShowWarningDialog() {
  importer_->Emit("show-warning-dialog");
}

ProfileWriter::~ProfileWriter() {}
