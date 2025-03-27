#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <gtk/gtk.h>
#include <sys/stat.h>
#include <glib.h>
#include <ctype.h>
#include <unistd.h>

#define ICON_DIR "/usr/share/icons/hicolor/"
#define PIXMAPS_DIR "/usr/share/pixmaps/"
#define APPLICATIONS_DIR "/usr/share/applications/"

int file_exists(const char *path)
{
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

char *get_icon_name_from_desktop(const char *app_id)
{
    char desktop_path[512];
    snprintf(desktop_path, sizeof(desktop_path), APPLICATIONS_DIR "%s.desktop", app_id);

    if (!file_exists(desktop_path))
        return NULL;

    GKeyFile *key_file = g_key_file_new();
    if (!g_key_file_load_from_file(key_file, desktop_path, G_KEY_FILE_NONE, NULL))
    {
        g_key_file_free(key_file);
        return NULL;
    }

    char *icon_name = g_key_file_get_string(key_file, "Desktop Entry", "Icon", NULL);
    g_key_file_free(key_file);

    return icon_name;
}

char *search_icon(const char *base_dir, const char *icon_name)
{
    DIR *dir = opendir(base_dir);
    if (!dir)
        return NULL;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_REG)
        {
            const char *filename = entry->d_name;

            char filename_no_ext[256];
            snprintf(filename_no_ext, sizeof(filename_no_ext), "%s", filename);
            char *dot = strrchr(filename_no_ext, '.');
            if (dot)
                *dot = '\0';

            if (strcasecmp(filename_no_ext, icon_name) == 0)
            {
                char *full_path = malloc(512);
                snprintf(full_path, 512, "%s%s", base_dir, filename);
                closedir(dir);
                return full_path;
            }
        }
    }

    closedir(dir);
    return NULL;
}

char *search_icon_in_hicolor(const char *icon_name)
{
    const char *sizes[] = {"scalable/apps/", "48x48/apps/", "32x32/apps/", "16x16/apps/", ""};
    char full_path[512];

    for (int i = 0; sizes[i][0] != '\0'; i++)
    {
        snprintf(full_path, sizeof(full_path), "%s%s", ICON_DIR, sizes[i]);
        char *icon_path = search_icon(full_path, icon_name);
        if (icon_path)
            return icon_path;
    }

    return NULL;
}

char *gtk_lookup_icon(const char *icon_name)
{
    GtkIconTheme *theme = gtk_icon_theme_get_for_display(gdk_display_get_default());
    GtkIconPaintable *icon_info = gtk_icon_theme_lookup_icon(theme, icon_name, NULL, 48, 1, GTK_TEXT_DIR_NONE, GTK_ICON_LOOKUP_PRELOAD);

    if (!icon_info)
        return NULL;

    GFile *file = gtk_icon_paintable_get_file(icon_info);
    if (!file)
        return NULL;

    char *filename = g_file_get_path(file);
    if (!filename)
        return NULL;

    g_object_unref(icon_info);
    return filename;
}

char *find_icon_path(const char *app_id)
{
    gtk_init();

    char *icon_name = get_icon_name_from_desktop(app_id);
    if (!icon_name)
        icon_name = g_strdup(app_id);

    char *icon_path = NULL;

    // GTK-based lookup
    icon_path = gtk_lookup_icon(icon_name);
    if (icon_path)
        goto cleanup;

    // /usr/share/icons/hicolor/*
    icon_path = search_icon_in_hicolor(icon_name);
    if (icon_path)
        goto cleanup;

    // /usr/share/pixmaps/
    icon_path = search_icon(PIXMAPS_DIR, icon_name);
    if (icon_path)
        goto cleanup;

cleanup:
    g_free(icon_name);
    return icon_path;
}
