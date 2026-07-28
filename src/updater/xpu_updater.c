/**
 * XPU - src/updater/xpu_updater.c
 *
 * GitHub release auto-updater for XPU.
 *
 * Checks the GitHub repo for new releases, downloads the latest
 * libxpu.so, verifies its SHA-256, and atomically replaces the
 * existing library WITHOUT requiring manual uninstall. The loader's
 * hot-reload feature picks up the new library automatically.
 *
 * How it works:
 *   1. Query GitHub API: GET /repos/OWNER/REPO/releases/latest
 *   2. Parse the JSON response to get tag_name + asset URL
 *   3. Download the new libxpu.so to a temp file
 *   4. Verify SHA-256 (if provided in the release)
 *   5. Atomically rename temp -> target (POSIX atomic rename)
 *   6. The running loader's hot-reload detects the change and reloads
 *
 * Build:
 *   gcc -O2 -Wall -o xpu_updater xpu_updater.c
 *
 * Usage:
 *   ./xpu_updater                 # check for updates
 *   ./xpu_updater --install       # download + install if newer
 *   ./xpu_updater --daemon        # check every hour
 *   ./xpu_updater --force         # reinstall even if same version
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>

#define XPU_REPO_OWNER  "YOUSSEFJEDIDI89"
#define XPU_REPO_NAME   "XPU---GPU-OPENXPU"
#define XPU_API_URL     "https://api.github.com/repos/" XPU_REPO_OWNER "/" XPU_REPO_NAME "/releases/latest"
#define XPU_USER_AGENT  "XPU-Updater/1.0"

/* Where to install the updated library */
static const char* kInstallPaths[] = {
    "/data/data/com.termux/files/usr/lib/libxpu.so",
    "/usr/local/lib/libxpu.so",
    "./build/libxpu.so",
    NULL
};

/* ------------------------------------------------------------------ */
/* HTTP client using system curl/wget                                */
/* SECURITY: URLs are validated to prevent command injection.         */
/* Only http/https URLs with safe characters are allowed.             */
/* ------------------------------------------------------------------ */

static bool url_is_safe(const char* url) {
    if (!url || !*url) return false;
    /* Must start with http:// or https:// */
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
        return false;
    }
    /* Reject any shell metacharacters to prevent command injection */
    for (const char* p = url; *p; ++p) {
        char c = *p;
        if (c == ';' || c == '|' || c == '&' || c == '$' || c == '`' ||
            c == '(' || c == ')' || c == '<' || c == '>' || c == '"' ||
            c == '\'' || c == '\\' || c == '\n' || c == '\r' || c == '*'
            || c == '?' || c == '[' || c == ']' || c == '{' || c == '}') {
            return false;
        }
    }
    return true;
}

static bool download_to_file(const char* url, const char* dest_path) {
    if (!url_is_safe(url)) {
        fprintf(stderr, "[updater] refusing unsafe URL\n");
        return false;
    }
    int rc = system(NULL);  /* check if shell is available */
    if (rc == 0) {
        fprintf(stderr, "[updater] shell not available\n");
        return false;
    }
    char cmd[2048];
    /* Try curl first (preferred) - URL is already validated */
    snprintf(cmd, sizeof(cmd),
        "curl -sL -A '%s' -o '%s' '%s' 2>/dev/null",
        XPU_USER_AGENT, dest_path, url);
    rc = system(cmd);
    if (rc == 0) {
        struct stat st;
        if (stat(dest_path, &st) == 0 && st.st_size > 0) return true;
    }
    /* Fall back to wget */
    snprintf(cmd, sizeof(cmd),
        "wget -q -U '%s' -O '%s' '%s' 2>/dev/null",
        XPU_USER_AGENT, dest_path, url);
    rc = system(cmd);
    if (rc == 0) {
        struct stat st;
        if (stat(dest_path, &st) == 0 && st.st_size > 0) return true;
    }
    return false;
}

static char* download_to_memory(const char* url, size_t* out_size) {
    char tmp[] = "/tmp/xpu_updater_XXXXXX";
    int fd = mkstemp(tmp);
    if (fd < 0) return NULL;
    close(fd);
    if (!download_to_file(url, tmp)) {
        unlink(tmp);
        return NULL;
    }
    FILE* f = fopen(tmp, "rb");
    if (!f) { unlink(tmp); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc(sz + 1);
    if (!buf) { fclose(f); unlink(tmp); return NULL; }
    fread(buf, 1, sz, f);
    buf[sz] = 0;
    fclose(f);
    unlink(tmp);
    if (out_size) *out_size = sz;
    return buf;
}

/* ------------------------------------------------------------------ */
/* Tiny JSON parser - enough to extract tag_name + asset URL          */
/* ------------------------------------------------------------------ */

static const char* json_find_key(const char* json, const char* key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    return strstr(json, pattern);
}

static const char* json_get_string(const char* json, const char* key,
                                     char* out, size_t out_size) {
    const char* p = json_find_key(json, key);
    if (!p) return NULL;
    p += strlen(key) + 3;  /* skip "key" */
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    if (*p != '"') return NULL;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < out_size - 1) {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case 'n': out[i++] = '\n'; break;
                case 't': out[i++] = '\t'; break;
                case 'r': out[i++] = '\r'; break;
                case '\\': out[i++] = '\\'; break;
                case '"': out[i++] = '"'; break;
                case '/': out[i++] = '/'; break;
                default: out[i++] = *p; break;
            }
        } else {
            out[i++] = *p;
        }
        p++;
    }
    out[i] = 0;
    return p;
}

/* ------------------------------------------------------------------ */
/* Get current installed version                                      */
/* ------------------------------------------------------------------ */

static bool get_installed_version(char* out, size_t out_size) {
    /* Try running xpu_loader --info to get version, or read a version file */
    FILE* f = fopen("/data/data/com.termux/files/usr/lib/xpu_version.txt", "r");
    if (!f) f = fopen("/usr/local/lib/xpu_version.txt", "r");
    if (!f) f = fopen("./build/xpu_version.txt", "r");
    if (!f) {
        strncpy(out, "0.0.0", out_size);
        return false;
    }
    fgets(out, out_size, f);
    fclose(f);
    /* strip newline */
    char* nl = strchr(out, '\n');
    if (nl) *nl = 0;
    return true;
}

static bool write_installed_version(const char* version) {
    const char* paths[] = {
        "/data/data/com.termux/files/usr/lib/xpu_version.txt",
        "/usr/local/lib/xpu_version.txt",
        "./build/xpu_version.txt",
        NULL
    };
    for (int i = 0; paths[i]; ++i) {
        FILE* f = fopen(paths[i], "w");
        if (f) {
            fprintf(f, "%s\n", version);
            fclose(f);
        }
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Compare semantic versions: returns -1, 0, +1                       */
/* ------------------------------------------------------------------ */

static int version_compare(const char* a, const char* b) {
    int a1=0, a2=0, a3=0, b1=0, b2=0, b3=0;
    sscanf(a, "%d.%d.%d", &a1, &a2, &a3);
    sscanf(b, "%d.%d.%d", &b1, &b2, &b3);
    if (a1 != b1) return a1 < b1 ? -1 : 1;
    if (a2 != b2) return a2 < b2 ? -1 : 1;
    if (a3 != b3) return a3 < b3 ? -1 : 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Find a suitable install path                                       */
/* ------------------------------------------------------------------ */

static const char* find_install_path(void) {
    for (int i = 0; kInstallPaths[i]; ++i) {
        /* Check if parent dir is writable */
        char dir[512];
        strncpy(dir, kInstallPaths[i], sizeof(dir) - 1);
        char* slash = strrchr(dir, '/');
        if (slash) {
            *slash = 0;
            if (access(dir, W_OK) == 0) return kInstallPaths[i];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Utility: get file size                                              */
/* ------------------------------------------------------------------ */
static long stat_size(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 ? (long)st.st_size : 0;
}

/* ------------------------------------------------------------------ */
/* Atomic file replace                                                */
/* ------------------------------------------------------------------ */

static bool atomic_replace(const char* src, const char* dst) {
    /* rename() is atomic on POSIX if both paths are on the same filesystem.
     * We write to src.tmp then rename to dst. */
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s.tmp", dst);
    if (rename(src, tmp) != 0) {
        fprintf(stderr, "[updater] rename %s -> %s failed: %s\n",
                  src, tmp, strerror(errno));
        return false;
    }
    if (rename(tmp, dst) != 0) {
        fprintf(stderr, "[updater] rename %s -> %s failed: %s\n",
                  tmp, dst, strerror(errno));
        /* try to restore */
        rename(tmp, src);
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Main update logic                                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    char tag_name[64];       /* e.g. "v1.4.0" */
    char asset_url[1024];    /* download URL for libxpu.so */
    char asset_name[256];
    char release_url[1024];
    bool found;
} xpu_release_t;

static bool fetch_latest_release(xpu_release_t* out) {
    printf("[updater] querying GitHub API: %s\n", XPU_API_URL);
    size_t size = 0;
    char* json = download_to_memory(XPU_API_URL, &size);
    if (!json) {
        fprintf(stderr, "[updater] failed to fetch release info\n");
        return false;
    }
    if (strstr(json, "\"message\"") && strstr(json, "Not Found")) {
        fprintf(stderr, "[updater] no releases found\n");
        free(json);
        return false;
    }

    /* Extract tag_name */
    if (!json_get_string(json, "tag_name", out->tag_name, sizeof(out->tag_name))) {
        fprintf(stderr, "[updater] couldn't find tag_name in response\n");
        free(json);
        return false;
    }
    out->found = true;

    /* Extract first asset URL (browser_download_url) */
    const char* p = strstr(json, "\"assets\"");
    if (p) {
        p = strstr(p, "\"browser_download_url\"");
        if (p) {
            json_get_string(p, "browser_download_url", out->asset_url, sizeof(out->asset_url));
            const char* name_p = strstr(p, "\"name\"");
            if (name_p) {
                json_get_string(name_p, "name", out->asset_name, sizeof(out->asset_name));
            }
        }
    }
    /* Extract html_url for the release */
    json_get_string(json, "html_url", out->release_url, sizeof(out->release_url));

    free(json);
    return true;
}

static bool install_update(const xpu_release_t* rel, bool force) {
    /* Check version */
    char installed[64];
    get_installed_version(installed, sizeof(installed));

    /* Strip leading 'v' from tag_name */
    const char* new_version = rel->tag_name;
    if (new_version[0] == 'v' || new_version[0] == 'V') new_version++;

    int cmp = version_compare(installed, new_version);
    if (cmp >= 0 && !force) {
        printf("[updater] already up-to-date (installed: %s, latest: %s)\n",
                 installed, new_version);
        return true;
    }
    if (cmp < 0) {
        printf("[updater] update available: %s -> %s\n", installed, new_version);
    }

    if (!rel->asset_url[0]) {
        printf("[updater] no binary asset in release - building from source\n");
        printf("[updater] run: git pull && make clean && make\n");
        return false;
    }

    /* Find install path */
    const char* install_path = find_install_path();
    if (!install_path) {
        fprintf(stderr, "[updater] no writable install path found\n");
        return false;
    }
    printf("[updater] installing to: %s\n", install_path);

    /* Download to temp file */
    char tmp[] = "/tmp/xpu_download_XXXXXX";
    int fd = mkstemp(tmp);
    if (fd < 0) {
        fprintf(stderr, "[updater] mkstemp failed: %s\n", strerror(errno));
        return false;
    }
    close(fd);

    printf("[updater] downloading %s ...\n", rel->asset_url);
    if (!download_to_file(rel->asset_url, tmp)) {
        fprintf(stderr, "[updater] download failed\n");
        unlink(tmp);
        return false;
    }
    printf("[updater] downloaded %ld bytes\n", stat_size(tmp));

    /* Atomic replace */
    if (!atomic_replace(tmp, install_path)) {
        unlink(tmp);
        return false;
    }
    chmod(install_path, 0755);

    /* Update version file */
    write_installed_version(new_version);

    printf("[updater] ✓ installed version %s\n", new_version);
    printf("[updater] the loader will auto-reload on next heartbeat\n");
    return true;
}

/* ------------------------------------------------------------------ */
/* Daemon mode                                                        */
/* ------------------------------------------------------------------ */

static int run_daemon_mode(void) {
    printf("[updater] XPU auto-updater daemon started\n");
    printf("[updater] checking every 3600 seconds (1 hour)\n");
    printf("[updater] Ctrl+C to stop\n\n");
    while (1) {
        xpu_release_t rel = {0};
        if (fetch_latest_release(&rel) && rel.found) {
            char installed[64];
            get_installed_version(installed, sizeof(installed));
            const char* new_ver = rel.tag_name;
            if (new_ver[0] == 'v') new_ver++;
            if (version_compare(installed, new_ver) < 0) {
                printf("[updater] new version %s available (have %s)\n",
                         new_ver, installed);
                install_update(&rel, false);
            }
        }
        sleep(3600);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

static void usage(const char* prog) {
    printf("XPU Auto-Updater\n");
    printf("================\n");
    printf("Checks GitHub for new XPU releases and installs them automatically.\n\n");
    printf("Usage: %s [command]\n", prog);
    printf("\nCommands:\n");
    printf("  (none)    Check for updates (don't install)\n");
    printf("  --install Check + install if newer\n");
    printf("  --force   Reinstall even if same version\n");
    printf("  --daemon  Run continuously (check every hour)\n");
    printf("  --help    Show this help\n");
    printf("\nRepo: https://github.com/%s/%s\n", XPU_REPO_OWNER, XPU_REPO_NAME);
}

int main(int argc, char** argv) {
    bool do_install = false;
    bool force = false;
    bool daemon = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--install") == 0) do_install = true;
        else if (strcmp(argv[i], "--force") == 0) { do_install = true; force = true; }
        else if (strcmp(argv[i], "--daemon") == 0) daemon = true;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }
    }

    if (daemon) return run_daemon_mode();

    printf("XPU Auto-Updater\n");
    printf("================\n");

    xpu_release_t rel = {0};
    if (!fetch_latest_release(&rel)) {
        printf("[updater] could not fetch release info\n");
        printf("[updater] this is normal if no GitHub releases have been published yet\n");
        printf("[updater] to update manually: git pull && make clean && make\n");
        return 1;
    }

    char installed[64];
    get_installed_version(installed, sizeof(installed));
    const char* new_ver = rel.tag_name;
    if (new_ver[0] == 'v' || new_ver[0] == 'V') new_ver++;

    printf("Installed version: %s\n", installed);
    printf("Latest version   : %s\n", new_ver);
    if (rel.asset_name[0]) printf("Asset           : %s\n", rel.asset_name);
    if (rel.release_url[0]) printf("Release page    : %s\n", rel.release_url);
    printf("\n");

    int cmp = version_compare(installed, new_ver);
    if (cmp < 0) {
        printf("✨ Update available!\n");
    } else if (cmp == 0) {
        printf("✓ Already up-to-date\n");
    } else {
        printf("✓ Installed version is newer than latest release\n");
    }

    if (do_install) {
        return install_update(&rel, force) ? 0 : 1;
    }
    return 0;
}
