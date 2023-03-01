// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <CL/sycl.hpp>

#if defined(EMBREE_SYCL_RT_VALIDATION_API)
#  include "../rttrace/rthwif_production.h"
#else
#  include "../rttrace/rthwif_production_igc.h"
#endif
#include "../rtbuild/rthwif_builder.h"

#include <level_zero/ze_api.h>

#include <vector>
#include <iostream>

#if !defined(_unused)
#define _unused(x) ((void)(x))
#endif

void exception_handler(sycl::exception_list exceptions)
{
  for (std::exception_ptr const& e : exceptions) {
    try {
      std::rethrow_exception(e);
    } catch(sycl::exception const& e) {
      std::cout << "Caught asynchronous SYCL exception: " << e.what() << std::endl;
    }
  }
};

inline void fwrite_uchar (unsigned char  v, std::fstream& file) { file.write((const char*)&v,sizeof(v)); }
inline void fwrite_ushort(unsigned short v, std::fstream& file) { file.write((const char*)&v,sizeof(v)); }

void storeTga(uint32_t* pixels, uint32_t width, uint32_t height, const std::string& fileName) try
{
  std::fstream file;
  file.exceptions (std::fstream::failbit | std::fstream::badbit);
  file.open (fileName.c_str(), std::fstream::out | std::fstream::binary);

  fwrite_uchar(0x00, file);
  fwrite_uchar(0x00, file);
  fwrite_uchar(0x02, file);
  fwrite_ushort(0x0000, file);
  fwrite_ushort(0x0000, file);
  fwrite_uchar(0x00, file);
  fwrite_ushort(0x0000, file);
  fwrite_ushort(0x0000, file);
  fwrite_ushort((unsigned short)width , file);
  fwrite_ushort((unsigned short)height, file);
  fwrite_uchar(0x18, file);
  fwrite_uchar(0x20, file);

  for (size_t y=0; y<height; y++) {
    for (size_t x=0; x<width; x++) {
      const uint32_t c = pixels[y*width+x];
      fwrite_uchar((unsigned char)((c>>0)&0xFF), file);
      fwrite_uchar((unsigned char)((c>>8)&0xFF), file);
      fwrite_uchar((unsigned char)((c>>16)&0xFF), file);
    }
  }
}
catch (std::exception const& e) {
  std::cout << "Error: Cannot write file " << fileName << std::endl;
  throw;
}

std::vector<char> readFile(const std::string& fileName) try
{
  std::fstream file;
  file.exceptions (std::fstream::failbit | std::fstream::badbit);
  file.open (fileName.c_str(), std::fstream::in | std::fstream::binary);

  file.seekg (0, std::ios::end);
  std::streampos size = file.tellg();
  std::vector<char> data(size);
  file.seekg (0, std::ios::beg);
  file.read (data.data(), size);
  file.close();

  return data;
}
catch (std::exception const& e) {
  std::cout << "Error: Cannot read file " << fileName << std::endl;
  throw;
}

bool compareTga(const std::string& fileNameA, const std::string& fileNameB)
{
  const std::vector<char> dataA = readFile(fileNameA);
  const std::vector<char> dataB = readFile(fileNameB);
  return dataA == dataB;
}

void* alloc_accel_buffer(size_t bytes, sycl::device device, sycl::context context)
{
  ze_context_handle_t hContext = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(context);
  ze_device_handle_t  hDevice  = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(device);
  
  ze_raytracing_mem_alloc_ext_desc_t rt_desc;
  rt_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_RAYTRACING_EXT_PROPERTIES;
  rt_desc.pNext = nullptr;
  rt_desc.flags = 0;
    
  ze_device_mem_alloc_desc_t device_desc;
  device_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
  device_desc.pNext = &rt_desc;
  device_desc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_CACHED;
  device_desc.ordinal = 0;

  ze_host_mem_alloc_desc_t host_desc;
  host_desc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
  host_desc.pNext = nullptr;
  host_desc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_CACHED;
  
  void* ptr = nullptr;
  ze_result_t result = zeMemAllocShared(hContext,&device_desc,&host_desc,bytes,RTHWIF_ACCELERATION_STRUCTURE_ALIGNMENT,hDevice,&ptr);
  assert(result == ZE_RESULT_SUCCESS);
  _unused(result);
  return ptr;
}

void free_accel_buffer(void* ptr, sycl::context context)
{
  ze_context_handle_t hContext = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(context);
  ze_result_t result = zeMemFree(hContext,ptr);
  assert(result == ZE_RESULT_SUCCESS);
  _unused(result);
}

#define bvh_bytes 4992

const unsigned char bvh_data[bvh_bytes] = {
  0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0x44,0x33,0x33,0x09,0x44,0xcd,0xcc,0x0b,0x44,
  0x02,0x00,0x00,0x00,0x0e,0x00,0x00,0x00,0x1d,0x00,0x00,0x00,0x3f,0x00,0x00,0x00,0x4e,0x00,0x00,0x00,0x4e,0x00,0x00,0x00,0x4e,0x00,0x00,0x00,0x4e,0x00,0x00,0x00,
  0x06,0x00,0x00,0x00,0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xb0,0xce,0x74,0x2f,0x7f,0x00,0x00,
  0xcd,0xcc,0x8b,0xb8,0xcd,0xcc,0x8b,0xb8,0xcd,0xcc,0x8b,0xb8,0x01,0x00,0x00,0x00,0x00,0x00,0x0a,0x0a,0x0a,0xff,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x89,0x42,0x00,
  0x42,0x14,0x8c,0x8c,0x6a,0x8c,0x77,0x49,0x00,0x00,0x00,0x00,0x00,0x00,0x8a,0x8a,0x53,0x8a,0x53,0x2a,0x8b,0x00,0x3d,0x00,0x3d,0x10,0x8c,0x8c,0x4b,0x8c,0x73,0x45,
  0xcd,0xcc,0x8b,0xb8,0xcd,0xcc,0x8b,0xb8,0xcc,0xcc,0x0b,0x44,0x1a,0x00,0x00,0x00,0x04,0x00,0x0a,0x0a,0xf4,0xff,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,
  0x80,0x80,0x8c,0x8a,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,0x80,0x80,0x8a,0x8a,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,0x80,0x80,0x80,0x80,0x00,0x00,0x00,0x00,
  0x65,0x66,0x09,0x44,0xcd,0xcc,0x8b,0xb8,0xcd,0xcc,0x8b,0xb8,0x1b,0x00,0x00,0x00,0x04,0x00,0x03,0x0a,0x0a,0xff,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x66,0x80,0x80,
  0x80,0x80,0xcd,0xcd,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,0x80,0x80,0x8a,0x8a,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,0x80,0x80,0x8c,0x8c,0x00,0x00,0x00,0x00,
  0xfe,0x7f,0x84,0x43,0x00,0x80,0x53,0xb8,0xfd,0xff,0x76,0x43,0x1c,0x00,0x00,0x00,0x04,0x00,0x08,0x09,0x06,0xff,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,
  0x80,0x80,0x9f,0x9f,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,0x80,0x80,0xa6,0xa6,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,0x80,0x80,0xc5,0xc5,0x00,0x00,0x00,0x00,
  0xcd,0xcc,0x8b,0xb8,0xcd,0xcc,0x8b,0xb8,0xcd,0xcc,0x8b,0xb8,0x1d,0x00,0x00,0x00,0x04,0x00,0x0a,0x0a,0x0a,0xff,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x8c,0x8c,0x8b,0x8b,0x01,0x01,0x89,0x89,0x00,0x00,0x00,0x00,0x8a,0x8a,0x01,0x01,0x8a,0x8a,0x00,0x00,0x00,0x00,0x00,0x00,0x8c,0x8c,0x8c,0x8c,0x8c,0x8c,
  0xfe,0x7f,0x84,0x43,0x00,0x00,0x6c,0xb8,0xfc,0xff,0x76,0x43,0x02,0x00,0x00,0x00,0x00,0x00,0x08,0x09,0x08,0xff,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x31,0x80,0x80,
  0x80,0x80,0xd0,0xd0,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,0x80,0x80,0xa6,0xa6,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,0x80,0x80,0xd2,0xd2,0x00,0x00,0x00,0x00,
  0xfb,0xff,0xa3,0x42,0x00,0x00,0x11,0xb8,0xfb,0xff,0x81,0x42,0x03,0x00,0x00,0x00,0x00,0x00,0x08,0x08,0x08,0xff,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x30,0x80,
  0x80,0x80,0x9f,0xd1,0xd1,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,0x80,0xa6,0xa6,0xa6,0x00,0x00,0x00,0xa0,0x00,0x00,0x80,0x80,0x80,0xd0,0xd0,0xd0,0x00,0x00,0x00,
  0xfe,0x7f,0x84,0x43,0x00,0x00,0x6c,0xb8,0xfc,0xff,0x76,0x43,0x20,0x00,0x00,0x00,0x04,0x00,0x08,0x09,0x08,0xff,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,
  0x00,0x80,0x32,0x32,0xd0,0x9f,0xd0,0x00,0x00,0x00,0x00,0xa5,0x00,0x80,0xa6,0xa6,0x01,0xa6,0x01,0x00,0x31,0x31,0x31,0x00,0x00,0x80,0xd2,0xd2,0xd2,0xd2,0xa0,0x00,
  0xfe,0xff,0x9c,0x43,0x00,0x00,0x6c,0xb8,0xfc,0xff,0x76,0x43,0x24,0x00,0x00,0x00,0x04,0x00,0x08,0x09,0x08,0xff,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x6d,0x6d,
  0x00,0x80,0x9f,0x9f,0x9f,0x9f,0x9f,0x00,0x00,0x00,0x00,0x00,0xa5,0x80,0xa6,0xa6,0xa6,0xa6,0xa6,0x00,0x9f,0x9f,0x00,0x00,0x00,0x80,0xd2,0xd2,0xa0,0xa0,0xd2,0x00,
  0xfc,0xff,0xa3,0x42,0x00,0x00,0x08,0xb8,0xfe,0xff,0x60,0x43,0x28,0x00,0x00,0x00,0x04,0x00,0x08,0x08,0x06,0xff,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,
  0x80,0x80,0x9f,0x9f,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,0x80,0x80,0xa6,0xa6,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,0x80,0x80,0xbd,0xbd,0x00,0x00,0x00,0x00,
  0xfb,0xff,0xa3,0x42,0x00,0x00,0x11,0xb8,0xfb,0xff,0x81,0x42,0x29,0x00,0x00,0x00,0x04,0x00,0x08,0x08,0x08,0xff,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,
  0x00,0x80,0x31,0x31,0xd1,0xd1,0x9f,0x00,0x00,0x00,0x00,0x00,0xa5,0x80,0xa6,0xa6,0x01,0x01,0xa6,0x00,0x00,0x00,0x00,0x31,0x00,0x80,0xa1,0xa1,0xa1,0xd0,0xd0,0x00,
  0xfe,0xff,0x01,0x43,0x00,0x00,0x11,0xb8,0xfb,0xff,0x81,0x42,0x2d,0x00,0x00,0x00,0x04,0x00,0x08,0x08,0x08,0xff,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x6e,0x6e,
  0x00,0x80,0xa1,0xa1,0xa1,0xa1,0xa1,0x00,0x00,0x00,0x00,0x00,0xa5,0x80,0xa6,0xa6,0xa6,0xa6,0xa6,0x00,0x00,0x00,0x31,0x31,0x00,0x80,0x32,0x32,0xd0,0xd0,0xd0,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x02,0x00,0x00,0xff,0x02,0x00,0x00,0x40,0x01,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x66,0x66,0x09,0x44,0x00,0x00,0x00,0x00,0xcd,0xcc,0x0b,0x44,0x00,0x00,0x00,0x00,
  0x33,0x33,0x09,0x44,0xcd,0xcc,0x0b,0x44,0x00,0x00,0x0b,0x44,0x33,0x33,0x09,0x44,0xcd,0xcc,0x0b,0x44,0x00,0x00,0x0b,0x44,0x33,0x33,0x09,0x44,0xcd,0xcc,0x0b,0x44,
  0x02,0x00,0x00,0xff,0x02,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x66,0x66,0x09,0x44,0x00,0x00,0x00,0x00,0xcd,0xcc,0x0b,0x44,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0xcd,0xcc,0x0b,0x44,0x00,0x00,0x00,0x00,0x33,0x33,0x09,0x44,0xcd,0xcc,0x0b,0x44,0x00,0x00,0x00,0x00,0x33,0x33,0x09,0x44,0xcd,0xcc,0x0b,0x44,
  0x04,0x00,0x00,0xff,0x04,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x33,0x33,0x0a,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x66,0x66,0x09,0x44,
  0x00,0x00,0x00,0x00,0xcd,0xcc,0x0b,0x44,0x00,0x00,0x0b,0x44,0x33,0x33,0x09,0x44,0xcd,0xcc,0x0b,0x44,0x00,0x00,0x0b,0x44,0x33,0x33,0x09,0x44,0xcd,0xcc,0x0b,0x44,
  0x04,0x00,0x00,0xff,0x04,0x00,0x00,0x40,0x01,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x33,0x33,0x0a,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0x44,
  0x33,0x33,0x09,0x44,0xcd,0xcc,0x0b,0x44,0x00,0x00,0x0b,0x44,0x33,0x33,0x09,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0x44,0x33,0x33,0x09,0x44,0x00,0x00,0x00,0x00,
  0x07,0x00,0x00,0xff,0x07,0x00,0x00,0x40,0x07,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x80,0x84,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x94,0x43,0x00,0x80,0xd3,0x43,
  0x00,0x00,0xa5,0x43,0x00,0x00,0x77,0x43,0x00,0x80,0xd3,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x77,0x43,0x00,0x80,0xd3,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x77,0x43,
  0x07,0x00,0x00,0xff,0x07,0x00,0x00,0x40,0x06,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x80,0x84,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x94,0x43,0x00,0x80,0x84,0x43,
  0x00,0x00,0xa5,0x43,0x00,0x00,0x94,0x43,0x00,0x80,0xd3,0x43,0x00,0x00,0xa5,0x43,0x00,0x00,0x77,0x43,0x00,0x80,0xd3,0x43,0x00,0x00,0xa5,0x43,0x00,0x00,0x77,0x43,
  0x01,0x00,0x00,0xff,0x01,0x00,0x00,0x40,0x01,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x0b,0x44,0x33,0x33,0x09,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x33,0x33,0x09,0x44,0xcd,0xcc,0x0b,0x44,0x00,0x00,0x00,0x00,0x33,0x33,0x09,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x33,0x33,0x09,0x44,0x00,0x00,0x00,0x00,
  0x01,0x00,0x00,0xff,0x01,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x0b,0x44,0x33,0x33,0x09,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0x44,
  0x33,0x33,0x09,0x44,0xcd,0xcc,0x0b,0x44,0x00,0x00,0x00,0x00,0x33,0x33,0x09,0x44,0xcd,0xcc,0x0b,0x44,0x00,0x00,0x00,0x00,0x33,0x33,0x09,0x44,0xcd,0xcc,0x0b,0x44,
  0x00,0x00,0x00,0xff,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x33,0x33,0x0a,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xcd,0xcc,0x0b,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xcd,0xcc,0x0b,0x44,
  0x00,0x00,0x00,0xff,0x00,0x00,0x00,0x40,0x01,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x33,0x33,0x0a,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0xcd,0xcc,0x0b,0x44,0x66,0x66,0x09,0x44,0x00,0x00,0x00,0x00,0xcd,0xcc,0x0b,0x44,0x66,0x66,0x09,0x44,0x00,0x00,0x00,0x00,0xcd,0xcc,0x0b,0x44,
  0x03,0x00,0x00,0xff,0x03,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xcd,0xcc,0x0b,0x44,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x33,0x33,0x09,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x33,0x33,0x09,0x44,0x00,0x00,0x00,0x00,
  0x03,0x00,0x00,0xff,0x03,0x00,0x00,0x40,0x01,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xcd,0xcc,0x0b,0x44,0x00,0x00,0x00,0x00,
  0x33,0x33,0x09,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x33,0x33,0x09,0x44,0xcd,0xcc,0x0b,0x44,0x00,0x00,0x00,0x00,0x33,0x33,0x09,0x44,0xcd,0xcc,0x0b,0x44,
  0x07,0x00,0x00,0xff,0x07,0x00,0x00,0x40,0x05,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x9d,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0xe4,0x43,0x00,0x80,0x84,0x43,
  0x00,0x00,0xa5,0x43,0x00,0x00,0x94,0x43,0x00,0x80,0x84,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x94,0x43,0x00,0x80,0x84,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x94,0x43,
  0x07,0x00,0x00,0xff,0x07,0x00,0x00,0x40,0x04,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x9d,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0xe4,0x43,0x00,0x00,0x9d,0x43,
  0x00,0x00,0xa5,0x43,0x00,0x00,0xe4,0x43,0x00,0x80,0x84,0x43,0x00,0x00,0xa5,0x43,0x00,0x00,0x94,0x43,0x00,0x80,0x84,0x43,0x00,0x00,0xa5,0x43,0x00,0x00,0x94,0x43,
  0x00,0x00,0x00,0xff,0x00,0x00,0x00,0x40,0x04,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0xec,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0xcb,0x43,0x00,0x00,0x9d,0x43,
  0x00,0x00,0x00,0x00,0x00,0x00,0xe4,0x43,0x00,0x80,0x84,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x94,0x43,0x00,0x80,0x84,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x94,0x43,
  0x06,0x00,0x00,0xff,0x06,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x80,0xd3,0x43,0x00,0x00,0xa5,0x43,0x00,0x00,0x77,0x43,0x00,0x80,0x84,0x43,
  0x00,0x00,0xa5,0x43,0x00,0x00,0x94,0x43,0x00,0x00,0x9d,0x43,0x00,0x00,0xa5,0x43,0x00,0x00,0xe4,0x43,0x00,0x00,0x9d,0x43,0x00,0x00,0xa5,0x43,0x00,0x00,0xe4,0x43,
  0x00,0x00,0x00,0xff,0x00,0x00,0x00,0x40,0x05,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0xec,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0xcb,0x43,0x00,0x80,0x84,0x43,
  0x00,0x00,0x00,0x00,0x00,0x00,0x94,0x43,0x00,0x80,0xd3,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x77,0x43,0x00,0x80,0xd3,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x77,0x43,
  0x07,0x00,0x00,0xff,0x07,0x00,0x00,0x40,0x02,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0xec,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0xcb,0x43,0x00,0x00,0xec,0x43,
  0x00,0x00,0xa5,0x43,0x00,0x00,0xcb,0x43,0x00,0x00,0x9d,0x43,0x00,0x00,0xa5,0x43,0x00,0x00,0xe4,0x43,0x00,0x00,0x9d,0x43,0x00,0x00,0xa5,0x43,0x00,0x00,0xe4,0x43,
  0x07,0x00,0x00,0xff,0x07,0x00,0x00,0x40,0x03,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0xec,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0xcb,0x43,0x00,0x00,0x9d,0x43,
  0x00,0x00,0xa5,0x43,0x00,0x00,0xe4,0x43,0x00,0x00,0x9d,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0xe4,0x43,0x00,0x00,0x9d,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0xe4,0x43,
  0x07,0x00,0x00,0xff,0x07,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x80,0xd3,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x77,0x43,0x00,0x80,0xd3,0x43,
  0x00,0x00,0xa5,0x43,0x00,0x00,0x77,0x43,0x00,0x00,0xec,0x43,0x00,0x00,0xa5,0x43,0x00,0x00,0xcb,0x43,0x00,0x00,0xec,0x43,0x00,0x00,0xa5,0x43,0x00,0x00,0xcb,0x43,
  0x07,0x00,0x00,0xff,0x07,0x00,0x00,0x40,0x01,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x80,0xd3,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x77,0x43,0x00,0x00,0xec,0x43,
  0x00,0x00,0xa5,0x43,0x00,0x00,0xcb,0x43,0x00,0x00,0xec,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0xcb,0x43,0x00,0x00,0xec,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0xcb,0x43,
  0x06,0x00,0x00,0xff,0x06,0x00,0x00,0x40,0x01,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x80,0xd3,0x43,0x00,0x00,0xa5,0x43,0x00,0x00,0x77,0x43,0x00,0x00,0x9d,0x43,
  0x00,0x00,0xa5,0x43,0x00,0x00,0xe4,0x43,0x00,0x00,0xec,0x43,0x00,0x00,0xa5,0x43,0x00,0x00,0xcb,0x43,0x00,0x00,0xec,0x43,0x00,0x00,0xa5,0x43,0x00,0x00,0xcb,0x43,
  0x05,0x00,0x00,0xff,0x05,0x00,0x00,0x40,0x08,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x70,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x88,0x43,0x00,0x00,0x70,0x43,
  0x00,0x00,0x25,0x43,0x00,0x00,0x88,0x43,0x00,0x00,0xa4,0x42,0x00,0x00,0x25,0x43,0x00,0x00,0x61,0x43,0x00,0x00,0xa4,0x42,0x00,0x00,0x25,0x43,0x00,0x00,0x61,0x43,
  0x05,0x00,0x00,0xff,0x05,0x00,0x00,0x40,0x09,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x70,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x88,0x43,0x00,0x00,0xa4,0x42,
  0x00,0x00,0x25,0x43,0x00,0x00,0x61,0x43,0x00,0x00,0xa4,0x42,0x00,0x00,0x00,0x00,0x00,0x00,0x61,0x43,0x00,0x00,0xa4,0x42,0x00,0x00,0x00,0x00,0x00,0x00,0x61,0x43,
  0x05,0x00,0x00,0xff,0x05,0x00,0x00,0x40,0x06,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0xa4,0x42,0x00,0x00,0x00,0x00,0x00,0x00,0x61,0x43,0x00,0x00,0xa4,0x42,
  0x00,0x00,0x25,0x43,0x00,0x00,0x61,0x43,0x00,0x00,0x02,0x43,0x00,0x00,0x25,0x43,0x00,0x00,0x82,0x42,0x00,0x00,0x02,0x43,0x00,0x00,0x25,0x43,0x00,0x00,0x82,0x42,
  0x05,0x00,0x00,0xff,0x05,0x00,0x00,0x40,0x07,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0xa4,0x42,0x00,0x00,0x00,0x00,0x00,0x00,0x61,0x43,0x00,0x00,0x02,0x43,
  0x00,0x00,0x25,0x43,0x00,0x00,0x82,0x42,0x00,0x00,0x02,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x82,0x42,0x00,0x00,0x02,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x82,0x42,
  0x00,0x00,0x00,0xff,0x00,0x00,0x00,0x40,0x03,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x91,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0xe4,0x42,0x00,0x00,0xa4,0x42,
  0x00,0x00,0x00,0x00,0x00,0x00,0x61,0x43,0x00,0x00,0x02,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x82,0x42,0x00,0x00,0x02,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x82,0x42,
  0x00,0x00,0x00,0xff,0x00,0x00,0x00,0x40,0x02,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x91,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0xe4,0x42,0x00,0x00,0x70,0x43,
  0x00,0x00,0x00,0x00,0x00,0x00,0x88,0x43,0x00,0x00,0xa4,0x42,0x00,0x00,0x00,0x00,0x00,0x00,0x61,0x43,0x00,0x00,0xa4,0x42,0x00,0x00,0x00,0x00,0x00,0x00,0x61,0x43,
  0x05,0x00,0x00,0xff,0x05,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x02,0x43,0x00,0x00,0x25,0x43,0x00,0x00,0x82,0x42,0x00,0x00,0xa4,0x42,
  0x00,0x00,0x25,0x43,0x00,0x00,0x61,0x43,0x00,0x00,0x70,0x43,0x00,0x00,0x25,0x43,0x00,0x00,0x88,0x43,0x00,0x00,0x70,0x43,0x00,0x00,0x25,0x43,0x00,0x00,0x88,0x43,
  0x05,0x00,0x00,0xff,0x05,0x00,0x00,0x40,0x05,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x02,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x82,0x42,0x00,0x00,0x91,0x43,
  0x00,0x00,0x25,0x43,0x00,0x00,0xe4,0x42,0x00,0x00,0x91,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0xe4,0x42,0x00,0x00,0x91,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0xe4,0x42,
  0x05,0x00,0x00,0xff,0x05,0x00,0x00,0x40,0x04,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x02,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x82,0x42,0x00,0x00,0x02,0x43,
  0x00,0x00,0x25,0x43,0x00,0x00,0x82,0x42,0x00,0x00,0x91,0x43,0x00,0x00,0x25,0x43,0x00,0x00,0xe4,0x42,0x00,0x00,0x91,0x43,0x00,0x00,0x25,0x43,0x00,0x00,0xe4,0x42,
  0x05,0x00,0x00,0xff,0x05,0x00,0x00,0x40,0x02,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x91,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0xe4,0x42,0x00,0x00,0x91,0x43,
  0x00,0x00,0x25,0x43,0x00,0x00,0xe4,0x42,0x00,0x00,0x70,0x43,0x00,0x00,0x25,0x43,0x00,0x00,0x88,0x43,0x00,0x00,0x70,0x43,0x00,0x00,0x25,0x43,0x00,0x00,0x88,0x43,
  0x05,0x00,0x00,0xff,0x05,0x00,0x00,0x40,0x03,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x91,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0xe4,0x42,0x00,0x00,0x70,0x43,
  0x00,0x00,0x25,0x43,0x00,0x00,0x88,0x43,0x00,0x00,0x70,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x88,0x43,0x00,0x00,0x70,0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x88,0x43,
  0x05,0x00,0x00,0xff,0x05,0x00,0x00,0x40,0x01,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x02,0x43,0x00,0x00,0x25,0x43,0x00,0x00,0x82,0x42,0x00,0x00,0x70,0x43,
  0x00,0x00,0x25,0x43,0x00,0x00,0x88,0x43,0x00,0x00,0x91,0x43,0x00,0x00,0x25,0x43,0x00,0x00,0xe4,0x42,0x00,0x00,0x91,0x43,0x00,0x00,0x25,0x43,0x00,0x00,0xe4,0x42,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

void render(unsigned int x, unsigned int y, void* bvh, unsigned int* pixels, unsigned int width, unsigned int height)
{
  intel_raytracing_ext_flag_t flags = intel_get_raytracing_ext_flag();
  if (!(flags & intel_raytracing_ext_flag_ray_query)) {
    pixels[y*width+x] = 0;
    return;
  }
  
  /* fixed camera */
  sycl::float3 vx(-1, -0, -0);
  sycl::float3 vy(-0, -1, -0);
  sycl::float3 vz(32, 32, 95.6379f);
  sycl::float3 p(278, 273, -800);

  /* compute primary ray */
  intel_ray_desc_t ray;
  ray.origin = p;
  ray.direction = float(x)*vx/8.0f + float(y)*vy/8.0f + vz;;
  ray.tmin = 0.0f;
  ray.tmax = INFINITY;
  ray.mask = 0xFF;
  ray.flags = intel_ray_flags_none;

  /* trace ray */
  intel_ray_query_t query = intel_ray_query_init(ray,(intel_raytracing_acceleration_structure_t)bvh);
  intel_ray_query_start_traversal(query);
  intel_ray_query_sync(query);

  /* get UVs of hit point */
  float u = 0, v = 0;
  if (intel_has_committed_hit(query))
  {
    sycl::float2 uv = intel_get_hit_barycentrics( query, intel_hit_type_committed_hit );
    u = uv.x();
    v = uv.y();
  }

  /* write color to framebuffer */
  sycl::float3 color(u,v,1.0f-u-v);
  unsigned int r = (unsigned int) (255.0f * color.x());
  unsigned int g = (unsigned int) (255.0f * color.y());
  unsigned int b = (unsigned int) (255.0f * color.z());
  pixels[y*width+x] = (b << 16) + (g << 8) + r;
}

int main(int argc, char* argv[])
{
  char* reference_img = NULL;
  if (argc > 2 && std::string(argv[1]) == std::string("--compare"))
  {
    reference_img = argv[2];
  }
  else
  {
    reference_img = (char*)"cornell_box_reference.tga";
  }

  sycl::device device = sycl::device(sycl::gpu_selector_v);
  sycl::queue queue = sycl::queue(device,exception_handler);
  sycl::context context = queue.get_context();

  void* bvh = alloc_accel_buffer(bvh_bytes,device,context);
  memcpy(bvh, bvh_data, bvh_bytes);

  static const int width = 512;
  static const int height = 512;
  unsigned int* pixels = (unsigned int*) sycl::aligned_alloc(64,width*height*sizeof(unsigned int),device,context,sycl::usm::alloc::shared);
  memset(pixels, 0, width*height*sizeof(uint32_t));

  for (int i=0; i<10; i++) {
  queue.submit([&](sycl::handler& cgh) {
                 const sycl::range<2> range(width,height);
                 cgh.parallel_for(range, [=](sycl::item<2> item) {
                                              const uint32_t x = item.get_id(0);
                                              const uint32_t y = item.get_id(1);
                                              render(x,y,bvh,pixels,width,height);
                                            });
               });
  queue.wait_and_throw();
  }

  free_accel_buffer(bvh,context);
  
  storeTga(pixels,width,height,"cornell_box.tga");
  const bool ok = compareTga("cornell_box.tga", reference_img);

  std::cout << "cornell_box ";
  if (ok) std::cout << "[PASSED]" << std::endl;
  else    std::cout << "[FAILED]" << std::endl;

  return ok ? 0 : 1;
}
