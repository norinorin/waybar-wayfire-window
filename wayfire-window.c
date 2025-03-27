#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <signal.h>

#include <wayland-client.h>
#include <wlr/util/log.h>
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
#include <cJSON.h>

#include "wayfire-ipc.h"
#include "find-icon.h"

#define GET_FOCUSED_VIEW "{\"method\": \"window-rules/get-focused-view\"}"
#define GET_FOCUSED_OUTPUT "{\"method\": \"window-rules/get-focused-output\"}"
#define BASE_DIR "/home/nori/playground/waybar-wayfire-window"
#define ASSETS_DIR BASE_DIR "/assets"

struct output_data;
struct toplevel_data;

struct output_data
{
    struct wl_output *output;
    char *name;
    char *description;
    struct wl_list link;
    struct toplevel_data *focused_view;
};

struct toplevel_data
{
    struct zwlr_foreign_toplevel_handle_v1 *toplevel;
    char *title;
    char *app_id;
    struct output_data *output;
    struct wl_list link;
};

static struct wl_list output_list;
static struct wl_list toplevel_list;

static struct wl_display *display = NULL;
static struct wl_registry *registry = NULL;
static struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager = NULL;
// static struct output_data *last_focused_output = NULL;

int copy_file(const char *source, const char *destination)
{
    int src = open(source, O_RDONLY);
    if (src < 0)
    {
        perror("Failed to open source file");
        return 1;
    }

    int dest = open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dest < 0)
    {
        perror("Failed to open destination file");
        close(src);
        return 1;
    }

    off_t offset = 0;
    struct stat stat_buf;
    fstat(src, &stat_buf);

    if (stat_buf.st_size < 0)
    {
        perror("size is negative");
        close(src);
        close(dest);
        return 1;
    }

    if (sendfile(dest, src, &offset, (size_t)stat_buf.st_size) == -1)
    {
        perror("sendfile error");
        close(src);
        close(dest);
        return 1;
    }

    close(src);
    close(dest);
    return 0;
}

static void write_atomic(const char *filename, const char *content)
{
    char temp_filename[64];
    snprintf(temp_filename, sizeof(temp_filename), "%s.tmp", filename);

    FILE *f = fopen(temp_filename, "w");
    if (!f)
    {
        wlr_log(WLR_ERROR, "fopen");
        return;
    }

    if (content)
        fprintf(f, "%s\n", content);

    fflush(f);
    fsync(fileno(f));
    fclose(f);

    if (rename(temp_filename, filename) != 0)
    {
        wlr_log(WLR_ERROR, "Error renaming temporary file");
        return;
    }
}

static void update_waybar(struct output_data *out)
{
    wlr_log(WLR_INFO, "Updating Waybar for output %s", out->name);

    char filename[64];
    const char *title = "";

    if (!out->name)
    {
        wlr_log(WLR_ERROR, "Output name is NULL");
        return;
    }

    snprintf(filename, sizeof(filename), "/tmp/sb-title-%s", out->name);

    if (out->focused_view)
        title = out->focused_view->title;

    wlr_log(WLR_INFO, "[%s] Active window: %s", out->name, title);
    wlr_log(WLR_DEBUG, "Writing to %s: %s", filename, title);

    write_atomic(filename, title);

    int icon_found = 0;
    snprintf(filename, sizeof(filename), "/tmp/sb-icon-%s", out->name);

    if (out->focused_view && out->focused_view->app_id)
    {
        wlr_log(WLR_INFO, "[%s] Active window app_id: %s", out->name, out->focused_view->app_id);
        char *icon = find_icon_path(out->focused_view->app_id);

        if (icon)
        {
            wlr_log(WLR_DEBUG, "Copying %s -> %s", icon, filename);
            copy_file(icon, filename);
            free(icon);
            icon_found = 1;
        }
    }

    if (!icon_found)
    {
        wlr_log(WLR_DEBUG, "Icon not found, placing a dummy transparent image to %s", filename);
        copy_file(ASSETS_DIR "/transparent.png", filename);
    }

    system("pkill -RTMIN+16 waybar");
}

static int is_focus_null()
{
    int result = 0;
    char *response = send_wayfire_ipc(GET_FOCUSED_VIEW);
    if (!response)
    {
        wlr_log(WLR_ERROR, "Failed to communicate with Wayfire IPC");
        return result;
    }

    cJSON *root = cJSON_Parse(response);
    if (!root)
    {
        wlr_log(WLR_ERROR, "JSON parsing error");
        goto cleanup;
    }

    cJSON *info = cJSON_GetObjectItem(root, "info");
    if (!info)
    {
        wlr_log(WLR_ERROR, "JSON parsing error");
        goto cleanup;
    }

    result = (int)(info->type == cJSON_NULL);

cleanup:
    cJSON_Delete(root);
    free(response);
    return result;
}

static struct output_data *get_focused_output()
{
    struct output_data *out = NULL;
    char *response = send_wayfire_ipc(GET_FOCUSED_OUTPUT);
    if (!response)
    {
        wlr_log(WLR_ERROR, "Failed to communicate with Wayfire IPC");
        return out;
    }

    cJSON *root = cJSON_Parse(response);
    if (!root)
    {
        wlr_log(WLR_ERROR, "JSON parsing error");
        goto cleanup;
    }

    cJSON *info = cJSON_GetObjectItem(root, "info");
    if (!info)
    {
        wlr_log(WLR_ERROR, "JSON parsing error");
        goto cleanup;
    }

    cJSON *name = cJSON_GetObjectItem(info, "name");
    if (!name)
    {
        wlr_log(WLR_ERROR, "JSON parsing error");
        goto cleanup;
    }

    struct output_data *tmp;
    wl_list_for_each(tmp, &output_list, link)
    {
        if (strcmp(tmp->name, name->valuestring) == 0)
        {
            wlr_log(WLR_INFO, "Found focused output: %s", tmp->name);
            out = tmp;
            break;
        }
    }

cleanup:
    cJSON_Delete(root);
    free(response);
    return out;
}

static void handle_output_name(void *data, struct wl_output *output, const char *name)
{
    struct output_data *out = data;
    free(out->name);
    out->name = strdup(name);
}

static void handle_output_geometry(void *data, struct wl_output *output,
                                   int32_t x, int32_t y, int32_t physical_width,
                                   int32_t physical_height, int32_t subpixel,
                                   const char *make, const char *model,
                                   int32_t transform)
{
}

static void handle_output_mode(void *data, struct wl_output *output,
                               uint32_t flags, int32_t width, int32_t height,
                               int32_t refresh)
{
}

static void handle_output_done(void *data, struct wl_output *output)
{
}

static void handle_output_scale(void *data, struct wl_output *output, int32_t factor)
{
}

static void handle_output_description(void *data, struct wl_output *output, const char *description)
{
    struct output_data *out = data;
    free(out->description);
    out->description = strdup(description);
    wlr_log(WLR_INFO, "Output detected: %s", description);
}

static const struct wl_output_listener output_listener = {
    .name = handle_output_name,
    .geometry = handle_output_geometry,
    .mode = handle_output_mode,
    .done = handle_output_done,
    .scale = handle_output_scale,
    .description = handle_output_description,
};

static void handle_toplevel_title(void *data,
                                  struct zwlr_foreign_toplevel_handle_v1 *toplevel,
                                  const char *title)
{
    struct toplevel_data *win = data;
    free(win->title);
    win->title = strdup(title);
    wlr_log(WLR_INFO, "Window title changed: %s", title);
    if (win->output && win->output->focused_view == win)
        update_waybar(win->output);
}

static void handle_toplevel_app_id(void *data,
                                   struct zwlr_foreign_toplevel_handle_v1 *toplevel,
                                   const char *app_id)
{
    struct toplevel_data *win = data;
    free(win->app_id);
    win->app_id = strdup(app_id);
    for (char *p = win->app_id; *p; ++p)
        *p = (char)tolower((unsigned char)*p);
    wlr_log(WLR_INFO, "Window app_id changed: %s", app_id);
}

static void handle_toplevel_done(void *data, struct zwlr_foreign_toplevel_handle_v1 *toplevel)
{
}

static void handle_output_enter(void *data,
                                struct zwlr_foreign_toplevel_handle_v1 *toplevel,
                                struct wl_output *output)
{
    wlr_log(WLR_DEBUG, "Output enter event");
    struct toplevel_data *win = data;
    struct output_data *out;

    wl_list_for_each(out, &output_list, link)
    {
        if (output == out->output)
        {
            win->output = out;
            win->output->focused_view = win;
            wlr_log(WLR_INFO, "Window '%s' moved to output %s", win->title, out->name);
            update_waybar(out);
            return;
        }
    }
}

static void handle_output_leave(void *data,
                                struct zwlr_foreign_toplevel_handle_v1 *toplevel,
                                struct wl_output *output)
{
    wlr_log(WLR_DEBUG, "Output leave event");
    struct toplevel_data *win;
    struct output_data *out;
    wl_list_for_each(win, &toplevel_list, link)
    {
        if (win->toplevel == toplevel && win->output && win->output->output == output)
        {
            win->output->focused_view = NULL;
            out = win->output;
            wlr_log(WLR_INFO, "Window '%s' left output %s", win->title, win->output->name);
            win->output = NULL;
            update_waybar(out);
            return;
        }
    }
}

static void handle_toplevel_closed(void *data, struct zwlr_foreign_toplevel_handle_v1 *toplevel)
{
    struct toplevel_data *win, *tmp;
    wl_list_for_each_safe(win, tmp, &toplevel_list, link)
    {
        if (win->toplevel == toplevel)
        {
            wlr_log(WLR_INFO, "Window '%s' closed.", win->title);
            // only update if this is the only window on the output
            // and so the workspace will be empty after this window is closed
            // otherwise, don't bother updating waybar as the next window will do it
            if (win->output && win == win->output->focused_view && win->output == get_focused_output() && is_focus_null())
            {
                win->output->focused_view = NULL;
                update_waybar(win->output);
            }
            wl_list_remove(&win->link);
            free(win->title);
            free(win->app_id);
            free(win);
            return;
        }
    }
}

static void handle_toplevel_parent(void *data,
                                   struct zwlr_foreign_toplevel_handle_v1 *toplevel,
                                   struct zwlr_foreign_toplevel_handle_v1 *parent)
{
}

static void handle_toplevel_state(void *data, struct zwlr_foreign_toplevel_handle_v1 *toplevel, struct wl_array *states)
{
    struct toplevel_data *win = data;
    uint32_t *state;
    int is_active = 0;

    wl_array_for_each(state, states)
    {
        if (*state == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
        {
            is_active = 1;
            break;
        }
    }

    if (is_active)
    {
        wlr_log(WLR_INFO, "Current output: %s", win->output->name ? win->output->name : "None");
        win->output->focused_view = win;
        update_waybar(win->output);
    }
    else if (is_focus_null())
    {
        struct output_data *out = get_focused_output();
        if (out)
        {
            wlr_log(WLR_INFO, "No focused view, updating Waybar for output %s", out->name);
            out->focused_view = NULL;
            update_waybar(out);
        }
    }

    wlr_log(WLR_INFO, "Window %s: %s (output: %s)", is_active ? "focused" : "unfocused", win->title, win->output ? win->output->name : "unknown");
}

static const struct zwlr_foreign_toplevel_handle_v1_listener toplevel_listener = {
    .title = handle_toplevel_title,
    .app_id = handle_toplevel_app_id,
    .state = handle_toplevel_state,
    .parent = handle_toplevel_parent,
    .output_enter = handle_output_enter,
    .output_leave = handle_output_leave,
    .closed = handle_toplevel_closed,
    .done = handle_toplevel_done,
};

static void handle_toplevel(void *data,
                            struct zwlr_foreign_toplevel_manager_v1 *manager,
                            struct zwlr_foreign_toplevel_handle_v1 *toplevel)
{
    struct toplevel_data *win = calloc(1, sizeof(struct toplevel_data));
    win->toplevel = toplevel;
    wl_list_insert(&toplevel_list, &win->link);

    wlr_log(WLR_INFO, "New window detected!");

    zwlr_foreign_toplevel_handle_v1_add_listener(toplevel, &toplevel_listener, win);
}

static const struct zwlr_foreign_toplevel_manager_v1_listener manager_listener = {
    .toplevel = handle_toplevel,
};

static void registry_handler(void *data, struct wl_registry *reg, uint32_t id,
                             const char *interface, uint32_t version)
{
    wlr_log(WLR_DEBUG, "Got a registry event for %s id %d", interface, id);

    if (strcmp(interface, wl_output_interface.name) == 0)
    {
        struct output_data *out = calloc(1, sizeof(struct output_data));
        out->output = wl_registry_bind(reg, id, &wl_output_interface, 4);
        wl_list_insert(&output_list, &out->link);
        wl_output_add_listener(out->output, &output_listener, out);
    }
    else if (strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0)
    {
        toplevel_manager = wl_registry_bind(reg, id, &zwlr_foreign_toplevel_manager_v1_interface, 3);
        zwlr_foreign_toplevel_manager_v1_add_listener(toplevel_manager, &manager_listener, NULL);
    }
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handler,
};

static void cleanup(void)
{
    struct output_data *out, *tmp_out;
    wl_list_for_each_safe(out, tmp_out, &output_list, link)
    {
        wl_list_remove(&out->link);
        free(out->name);
        free(out->description);
        free(out);
    }

    struct toplevel_data *win, *tmp_win;
    wl_list_for_each_safe(win, tmp_win, &toplevel_list, link)
    {
        wl_list_remove(&win->link);
        free(win->title);
        free(win->app_id);
        free(win);
    }

    close_wayfire_ipc_connection();
}

int main()
{
    if (prctl(PR_SET_PDEATHSIG, SIGTERM) == -1)
    {
        perror("prctl");
        exit(EXIT_FAILURE);
    }

    wlr_log_init(WLR_DEBUG, NULL);

    wl_list_init(&output_list);
    wl_list_init(&toplevel_list);

    display = wl_display_connect(NULL);
    if (!display)
    {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return 1;
    }

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (open_wayfire_ipc_connection() != 0)
    {
        wlr_log(WLR_ERROR, "Failed to open Wayfire IPC connection");
        return 1;
    }

    while (wl_display_dispatch(display) != -1)
    {
    }

    cleanup();
    wl_display_disconnect(display);
    return 0;
}
