#include "gpu.h"

static void gpu_panic()
{
    sgl_log("Something went wrong with OpenCL.\n");
    exit(EXIT_FAILURE);
}

static void CL_CALLBACK gpui_context_notify(const char* errinfo, const void* db, size_t s, void* ud)
{
    sgl_log("OpenCL context failure notification: %s\n", errinfo);
    exit(EXIT_FAILURE);
}

b32 gpu_init(GPUInfo* gpu_info)
{
    b32 ok = true;
#define ERR_CHECK if ( err != CL_SUCCESS ) { ok = false; goto end; }
    cl_int err = CL_SUCCESS;

    cl_uint num_platforms = 0;
    err = clGetPlatformIDs(0, NULL, &num_platforms);
    ERR_CHECK;
    cl_platform_id*   platforms   = sgl_calloc(sizeof(cl_platform_id), num_platforms);
    clGetPlatformIDs(num_platforms, platforms, NULL);

    for ( cl_uint pi = 0; pi < num_platforms; ++pi ) {
        cl_platform_id platform = platforms[pi];

        cl_platform_info requested_infos [] = {
            CL_PLATFORM_PROFILE,
            CL_PLATFORM_VERSION,
            CL_PLATFORM_NAME,
            CL_PLATFORM_VENDOR,
        };
        char* requested_infos_str [] = {
            "CL_PLATFORM_PROFILE",
            "CL_PLATFORM_VERSION",
            "CL_PLATFORM_NAME",
            "CL_PLATFORM_VENDOR",
        };
        for ( uint32_t i = 0; i < sgl_array_count(requested_infos); ++i ) {
            size_t str_len = 0;
            clGetPlatformInfo(platform, requested_infos[i], 0, NULL, &str_len);
            if ( str_len > 0 ) {
                char* info = sgl_calloc(sizeof(str_len) + 1, 1);
                clGetPlatformInfo(platform, requested_infos[i], str_len, info, NULL);
                sgl_log("CL INFO: %s -- %s\n", requested_infos_str[i], info);
                sgl_free(info);
            }
        }
    }

    if (num_platforms > 1) {
        sgl_log("Using platform 0 even though there are more than one.\n");
    }

    cl_uint num_devices = 0;
    if ( clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_ALL, 0, NULL, &num_devices) != CL_SUCCESS ) {
        gpu_panic();
    }

    cl_device_id* devices = sgl_calloc(sizeof(cl_device_id), num_devices);
    err = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_ALL, num_devices, devices, 0);
    ERR_CHECK;

    // TODO: use clGetDeviceInfo if we ever need to know all the infos
    if (num_devices > 1) {
        sgl_log ( "More than one device available...\n" );
    }


    cl_context_properties context_properties[] = {
        CL_CONTEXT_PLATFORM, (cl_context_properties)(platforms[0]),
        0,
    };
    gpu_info->context = clCreateContext(context_properties,
                                        num_devices,
                                        devices,
                                        NULL, // CALLBACK
                                        //gpui_context_notify,
                                        NULL, /* user_data */
                                        &err);
    ERR_CHECK;

end:
    sgl_free(platforms);
    sgl_free(devices);
    return ok;
#undef ERR_CHECK
}
