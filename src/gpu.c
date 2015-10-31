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

static void handle_cl_error(cl_int err)
{
    switch(err) {
    case CL_INVALID_VALUE:
        sgl_log("%s\n", "CL_INVALID_VALUE");
        break;
    case CL_INVALID_DEVICE:
        sgl_log("%s\n", "CL_INVALID_DEVICE");
        break;
    case CL_INVALID_BINARY:
        sgl_log("%s\n", "CL_INVALID_BINARY");
        break;
    case CL_INVALID_BUILD_OPTIONS:
        sgl_log("%s\n", "CL_INVALID_BUILD_OPTIONS");
        break;
    case CL_INVALID_OPERATION:
        sgl_log("%s\n", "CL_INVALID_OPERATION");
        break;
    case CL_COMPILER_NOT_AVAILABLE:
        sgl_log("%s\n", "CL_COMPILER_NOT_AVAILABLE");
        break;
    case CL_BUILD_PROGRAM_FAILURE:
        sgl_log("%s\n", "CL_BUILD_PROGRAM_FAILURE");
        break;
    case CL_OUT_OF_RESOURCES:
        sgl_log("%s\n", "CL_OUT_OF_RESOURCES");
        break;
    case CL_OUT_OF_HOST_MEMORY:
        sgl_log("%s\n", "CL_OUT_OF_HOST_MEMORY");
        break;
    }
}

b32 gpu_init(GPUInfo* gpu_info)
{
#define ERR_CHECK if ( err != CL_SUCCESS ) { ok = false; handle_cl_error(err); goto end; }
    b32 ok = true;
    cl_int err = CL_SUCCESS;

    cl_uint num_platforms = 0;
    if ( clGetPlatformIDs(0, NULL, &num_platforms) != CL_SUCCESS ) {
        gpu_panic();
    }

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

    // Compile the program.
    int64_t file_contents_count;
    // Assuming that we are running from the root project directory...
    char* file_contents =  sgl_slurp_file("src/opencl_jpeg.cl", &file_contents_count);
    if (!file_contents) {
        goto end;
    }

    cl_program program = clCreateProgramWithSource(gpu_info->context,
                                                   1,
                                                   &file_contents,
                                                   NULL,
                                                   &err);

    sgl_free(file_contents);

    ERR_CHECK;

    err = clBuildProgram(program, 1, &devices[0],
                         "-cl-std=CL1.1",  // build options...
                         NULL, // callback
                         NULL // user data
                        );

    if (err == CL_BUILD_PROGRAM_FAILURE) {
        size_t sz = 0;
        clGetProgramBuildInfo(program,
                              devices[0],
                              CL_PROGRAM_BUILD_LOG,
                              0,
                              NULL,
                              &sz);
        char* log = sgl_malloc(sz);
        clGetProgramBuildInfo(program,
                              devices[0],
                              CL_PROGRAM_BUILD_LOG,
                              sz,
                              log,
                              NULL);
        sgl_log ("%s\n", log);
        sgl_free(log);
    }

    ERR_CHECK;




end:
    sgl_free(platforms);
    sgl_free(devices);
    return ok;
#undef ERR_CHECK
}

// Passes in the huffman table for luma dc buffers. 1/6th of the data that the
// actual JPEG encoder would use, but it's the one we use.
void gpu_setup_buffers(GPUInfo* gpu_info, uint8_t* huffsize, uint16_t* huffcode)
{
    // TODO: Implement.
}
