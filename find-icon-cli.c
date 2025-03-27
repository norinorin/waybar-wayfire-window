#include <stdio.h>
#include <stdlib.h>

#include "find-icon.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s <app_id>\n", argv[0]);
        return 1;
    }

    char *icon_path = find_icon_path(argv[1]);
    if (icon_path)
    {
        printf("Found icon for %s: %s\n", argv[1], icon_path);
        free(icon_path);
        return 0;
    }

    printf("Icon not found\n");
    return 1;
}