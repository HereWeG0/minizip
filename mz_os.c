/* mz_os.c -- System functions
   Version 2.3.9, July 26, 2018
   part of the MiniZip project

   Copyright (C) 2010-2018 Nathan Moinvaziri
     https://github.com/nmoinvaz/minizip
   Copyright (C) 1998-2010 Gilles Vollant
     http://www.winimage.com/zLibDll/minizip.html

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "mz.h"
#include "mz_os.h"
#include "mz_strm.h"
#include "mz_strm_crc32.h"

/***************************************************************************/

int32_t mz_make_dir(const char *path)
{
    int32_t err = MZ_OK;
    int16_t len = 0;
    char *current_dir = NULL;
    char *match = NULL;
    char hold = 0;


    len = (int16_t)strlen(path);
    if (len <= 0)
        return 0;

    current_dir = (char *)MZ_ALLOC(len + 1);
    if (current_dir == NULL)
        return MZ_MEM_ERROR;

    strcpy(current_dir, path);

    if (current_dir[len - 1] == '/')
        current_dir[len - 1] = 0;

    err = mz_os_make_dir(current_dir);
    if (err != MZ_OK)
    {
        match = current_dir + 1;
        while (1)
        {
            while (*match != 0 && *match != '\\' && *match != '/')
                match += 1;
            hold = *match;
            *match = 0;

            err = mz_os_make_dir(current_dir);
            if (err != MZ_OK)
                break;
            if (hold == 0)
                break;

            *match = hold;
            match += 1;
        }
    }

    MZ_FREE(current_dir);
    return err;
}

int32_t mz_path_combine(char *path, const char *join, int32_t max_path)
{
    int32_t path_len = 0;

    if (path == NULL || join == NULL || max_path == 0)
        return MZ_PARAM_ERROR;

    path_len = strlen(path);

    if (path_len == 0)
    {
        strncpy(path, join, max_path);
    }
    else
    {
        if (path[path_len - 1] != '\\' && path[path_len - 1] != '/')
            strncat(path, "/", max_path - path_len - 1);
        strncat(path, join, max_path - path_len);
    }

    return MZ_OK;
}

int32_t mz_path_resolve(const char *path, char *output, int32_t max_output)
{
    const char *source = path;
    const char *check = output;
    char *target = output;

    if (max_output <= 0)
        return MZ_PARAM_ERROR;

    while (*source != 0 && max_output > 1)
    {
        check = source;
        if ((*check == '\\') || (*check == '/'))
            check += 1;

        if ((source == path) || (check != source) || (*target == 0))
        {
            // Skip double paths
            if ((*check == '\\') || (*check == '/'))
            {
                source += 1;
                continue;
            }
            if ((*check != 0) && (*check == '.'))
            {
                check += 1;

                // Remove current directory . if at end of stirng
                if ((*check == 0) && (source != path))
                {
                    // Copy last slash
                    *target = *source;
                    target += 1;
                    max_output -= 1;
                    source += (check - source);
                    continue;
                }

                // Remove current directory . if not at end of stirng
                if ((*check == 0) || (*check == '\\' || *check == '/'))
                {                   
                    // Only proceed if .\ is not entire string
                    if (check[1] != 0 || (path != source))
                    {
                        source += (check - source);
                        continue;
                    }
                }

                // Go to parent directory ..
                if ((*check != 0) || (*check == '.'))
                {
                    check += 1;
                    if ((*check == 0) || (*check == '\\' || *check == '/'))
                    {
                        source += (check - source);

                        // Search backwards for previous slash
                        if (target != output)
                        {
                            target -= 1;
                            do
                            {
                                if ((*target == '\\') || (*target == '/'))
                                    break;

                                target -= 1;
                                max_output += 1;
                            }
                            while (target > output);
                        }
                        
                        if ((target == output) && (*source != 0))
                            source += 1;
                        if ((*target == '\\' || *target == '/') && (*source == 0))
                            target += 1;

                        *target = 0;
                        continue;
                    }
                }
            }
        }

        *target = *source;

        source += 1;
        target += 1;
        max_output -= 1;
    }

    *target = 0;

    if (*path == 0)
        return MZ_INTERNAL_ERROR;

    return MZ_OK;
}

int32_t mz_path_remove_filename(const char *path)
{
    char *path_ptr = NULL;

    if (path == NULL)
        return MZ_PARAM_ERROR;

    path_ptr = (char *)(path + strlen(path) - 1);

    while (path_ptr > path)
    {
        if ((*path_ptr == '/') || (*path_ptr == '\\'))
        {
            *path_ptr = 0;
            break;
        }

        path_ptr -= 1;
    }
    return MZ_OK;
}

int32_t mz_path_get_filename(const char *path, const char **filename)
{
    const char *match = NULL;

    if (path == NULL || filename == NULL)
        return MZ_PARAM_ERROR;

    *filename = NULL;

    for (match = path; *match != 0; match += 1)
    {
        if ((*match == '\\') || (*match == '/'))
            *filename = match + 1;
    }

    if (*filename == NULL)
        return MZ_EXIST_ERROR;

    return MZ_OK;
}

int32_t mz_get_file_crc(const char *path, uint32_t *result_crc)
{
    void *stream = NULL;
    void *crc32_stream = NULL;
    int32_t read = 0;
    int32_t err = MZ_OK;
    uint8_t buf[INT16_MAX];

    mz_stream_os_create(&stream);

    err = mz_stream_os_open(stream, path, MZ_OPEN_MODE_READ);

    mz_stream_crc32_create(&crc32_stream);
    mz_stream_crc32_open(crc32_stream, NULL, MZ_OPEN_MODE_READ);

    mz_stream_set_base(crc32_stream, stream);

    if (err == MZ_OK)
    {
        do
        {
            read = mz_stream_crc32_read(crc32_stream, buf, sizeof(buf));

            if (read < 0)
            {
                err = read;
                break;
            }
        }
        while ((err == MZ_OK) && (read > 0));

        mz_stream_os_close(stream);
    }

    mz_stream_crc32_close(crc32_stream);
    *result_crc = mz_stream_crc32_get_value(crc32_stream);
    mz_stream_crc32_delete(&crc32_stream);

    mz_stream_os_delete(&stream);

    return err;
}
