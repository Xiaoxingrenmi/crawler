
// Url Map Utils
//   by BOT Man & ZhangHan, 2018

#include "url_map.h"

#include <assert.h>

// use C++ string & multimap to store url mapping
#include <map>
#include <string>

size_t yield_unique_index() {
  static size_t index = 0;
  return ++index;
}

// url -> url's index
typedef std::map<std::string, size_t> IndexMap;
IndexMap& g_index_map() {
  static IndexMap index_map;
  return index_map;
}

size_t GetIndex(const std::string& url) {
  IndexMap::iterator iter = g_index_map().find(url);
  if (iter != g_index_map().end())
    return iter->second;

  size_t new_index = yield_unique_index();
  g_index_map().insert(IndexMap::value_type(url, new_index));
  return new_index;
}

// url's index -> [ connected urls' index ]
typedef std::multimap<size_t, size_t> UrlMap;
UrlMap& g_url_map() {
  static UrlMap url_map;
  return url_map;
}

void ConnectUrls(const char* src, const char* dst) {
  g_url_map().insert(UrlMap::value_type(GetIndex(src), GetIndex(dst)));
}

void YieldUrlConnectionIndex(yeild_url_connection_index_callback_fn callback,
                             void* context) {
  for (IndexMap::const_iterator iter = g_index_map().begin();
       iter != g_index_map().end(); ++iter) {
    callback(iter->first.c_str(), iter->second, context);
  }
}

void YieldUrlConnectionPair(yeild_url_connection_pair_callback_fn callback,
                            void* context) {
  for (UrlMap::const_iterator iter = g_url_map().begin();
       iter != g_url_map().end(); ++iter) {
    callback(iter->first, iter->second, context);
  }
}
