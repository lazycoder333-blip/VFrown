#include "common.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#ifdef _WIN32
#include <direct.h>
#endif

bool strequ(const char* a, const char* b) {
  if (a && !b)
    return false;

  if (!a && b)
    return false;

  if (!a && !b)
    return true;

  uint32_t i = 0;
  while (true) {
    if (a[i] == '\0' || b[i] == '\0')
      break;

    if (a[i] != b[i])
      return false;

    i++;
  }

  if (a[i] == b[i])
    return true;

  return false;
}


bool EnsureDirectoryExists(const char* path) {
  if (!path || path[0] == '\0') return false;

  // Make a mutable copy
  char tmp[1024];
  strncpy(tmp, path, sizeof(tmp));
  tmp[sizeof(tmp)-1] = '\0';

  size_t len = strlen(tmp);
  if (len == 0) return false;

  for (size_t i = 1; i <= len; i++) {
    if (tmp[i] == '\0' || tmp[i] == PATH_CHAR) {
      char old = tmp[i];
      tmp[i] = '\0';

      struct stat st;
      if (stat(tmp, &st) != 0) {
        if (errno == ENOENT) {
#ifdef _WIN32
          if (_mkdir(tmp) != 0) {
            tmp[i] = old;
            return false;
          }
#else
          if (mkdir(tmp, 0755) != 0) {
            tmp[i] = old;
            return false;
          }
#endif
        } else {
          tmp[i] = old;
          return false;
        }
      } else {
        if (!S_ISDIR(st.st_mode)) {
          tmp[i] = old;
          return false;
        }
      }

      tmp[i] = old;
    }
  }

  // final check
  struct stat stfinal;
  if (stat(path, &stfinal) == 0 && S_ISDIR(stfinal.st_mode)) return true;
  return false;
}
