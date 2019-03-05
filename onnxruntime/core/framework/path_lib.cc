// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "path_lib.h"
#include "core/common/status.h"
#include "core/common/common.h"
#include <pathcch.h>
#include <assert.h>
#include <shlwapi.h>

namespace onnxruntime {
namespace {
//starting from pszPath_end-1, backsearch the first char that is not L'/'
inline void backsearch(PWSTR pszPath, PWSTR& pszPath_end) {
  PWSTR const pszPath_second = pszPath + 1;
  while (pszPath_end > pszPath_second && *(pszPath_end - 1) == L'/')
    --pszPath_end;
  *pszPath_end = L'\0';
}

inline Status RemoveFileSpec(PWSTR pszPath, size_t cchPath) {
  assert(pszPath != nullptr && pszPath[0] != L'\0');
  if (PathIsUNCW(pszPath) == TRUE) {
    return Status(common::ONNXRUNTIME, common::NOT_IMPLEMENTED, "UNC path is not supported yet");
  }
  for (PWSTR t = L"\0"; *t == L'\0'; t = PathRemoveBackslashW(pszPath))
    ;
  PWSTR pszLast = PathSkipRootW(pszPath);
  if (pszLast == nullptr) pszLast = pszPath;
  if (*pszLast == L'\0') {
    return Status::OK();
  }
  PWSTR beginning_of_the_last = pszLast;
  for (PWSTR t;; beginning_of_the_last = t) {
    t = PathFindNextComponentW(beginning_of_the_last);
    if (t == nullptr) {
      return Status(common::ONNXRUNTIME, common::FAIL, "unexpected failure");
    }
    if (*t == L'\0')
      break;
  }
  *beginning_of_the_last = L'\0';
  if (*pszPath == L'\0') {
    pszPath[0] = L'.';
    pszPath[1] = L'\0';
  } else
    for (PWSTR t = L"\0"; *t == L'\0'; t = PathRemoveBackslashW(pszPath))
      ;
  return Status::OK();
}
}  // namespace
common::Status GetDirNameFromFilePath(const std::basic_string<ORTCHAR_T>& s, std::basic_string<ORTCHAR_T>& ret) {
  std::wstring input = s;
  if (input.empty()) {
    ret = ORT_TSTR(".");
    return Status::OK();
  }
  ret = s;
  auto st = onnxruntime::RemoveFileSpec(const_cast<wchar_t*>(ret.data()), ret.length() + 1);
  if (!st.IsOK()) {
    std::ostringstream oss;
    oss << "illegal input path:", ToMBString(s);
    return Status(st.Category(), st.Code(), oss.str());
  }
  ret.resize(wcslen(ret.c_str()));
  return Status::OK();
}
}  // namespace onnxruntime
