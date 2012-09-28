/*
 *  Copyright (C) 2009-2012 Texas Instruments, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _DVP_MEM_H_
#define _DVP_MEM_H_

/*! \file
 * \brief The DVP Memory API.
 * \author Erik Rainey <erik.rainey@ti.com>
 */

#include <dvp/dvp_types.h>
#include <sosal/fourcc.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \brief Unmaps and frees memory from remote cores.
 * \param [in] handle The handle to the DVP system.
 * \param [in] ptr The pointer to free.
 * \param [in] numElem The number of elements in the array.
 * \param [in] sizeElem The size of each element.
 * \ingroup group_memory
 */
void dvp_free(DVP_Handle handle, DVP_PTR ptr, DVP_U32 numElem, DVP_U32 sizeElem);

/*!
 * \brief Allocates and clears virtual memory, internally takes care of mapping to remote cores.
 * \param [in] handle The handle to the DVP system.
 * \param [in] numElem The number of elements to allocate.
 * \param [in] sizeElem The size of each element in bytes.
 * \ingroup group_memory
 */
DVP_PTR dvp_calloc(DVP_Handle handle, DVP_U32 numElem, DVP_U32 sizeElem);

/*!
 * \brief Initializes the image structure to the correct parameters for the color format.
 * \note Does not allocate any memory!
 * \param [in] pImage The pointer to the structure to be filled in.
 * \param [in] width The desired width in pixels.
 * \param [in] height The desired height in pixels.
 * \param [in] fourcc The desired color space.
 * \ingroup group_images
 */
void DVP_Image_Init(DVP_Image_t *pImage, DVP_U32 width, DVP_U32 height, fourcc_t fourcc);

/*!
 * \brief Allocates and maps an image to all enabled remote cores.
 * \param [in] handle The handle to DVP.
 * \param [in] pImage The pointer to the initialized image structure.
 * \param [in] dvpMemType The desired memory allocation type.
 * \note Display buffers are already allocated if acquired through DVP_Display_Alloc. Use an appropriate mem type which will not cause a reallocation.
 * \ingroup group_images
 */
DVP_BOOL DVP_Image_Alloc(DVP_Handle handle, DVP_Image_t *pImage, DVP_MemType_e dvpMemType);

/*!
 * \brief Unmaps and Frees the allocated memory in the image.
 * \param [in] handle The handle to DVP.
 * \param [in] pImage The pointer to the image structure whose data will be freed. The structure will NOT be cleared afterward.
 * \ingroup group_images
 */
DVP_BOOL DVP_Image_Free(DVP_Handle handle, DVP_Image_t *pImage);

/*!
 * \brief Performs the necessary operations to share this image with another process
 * \param [in] handle The handle to DVP.
 * \param [in] pImage The pointer to the image structure whose data will be shared.
 * \param [in] shared_fds the file descriptors that can be used to import this image in a different process
 * \note the shared_fds array size must match the number of planes in the image
 * \ingroup group_images
 */
DVP_BOOL DVP_Image_Share(DVP_Handle handle, DVP_Image_t *pImage, DVP_S32 *shared_fds);

/*!
 * \brief Import the image into the calling process
 * \param [in] handle The handle to DVP.
 * \param [in] pImage The pointer to the image structure whose data will be imported.
 * \param [in] shared_fds the file descriptors obtained from DVP_Image_Share
 * \param [in] hdls internal handles, needed to free the image through DVP_Image_Free_Import
 * \note the hdls array size must match the number of planes in the image
 * \note This image can only be freed through DVP_Image_Free_Import
 * \ingroup group_images
 */
DVP_BOOL DVP_Image_Import(DVP_Handle handle, DVP_Image_t *pImage, DVP_S32 *shared_fds, DVP_VALUE *hdls);

/*!
 * \brief Free an imported image
 * \param [in] handle The handle to DVP.
 * \param [in] pImage The pointer to the image structure whose data will be freed.
 * \param [in] hdls the internal handles obtained during DVP_Image_Import
 * \note only imported images can be freed through this API
 * \ingroup group_images
 */
DVP_BOOL DVP_Image_Free_Import(DVP_Handle handle, DVP_Image_t *pImage, DVP_VALUE *hdls);

/*!
 * \brief Deinitializes the image structure.
 * \param [in] pImage the data structure to clear.
 * \ingroup group_images
 */
void DVP_Image_Deinit(DVP_Image_t *pImage);

/*!
 * \brief Copies a flat buffer into a DVP Image structure.
 * \param pImage The image to fill.
 * \param ptr The pointer to the data to fill from.
 * \param size The number of bytes in ptr.
 * \ingroup group_images
 */
void DVP_Image_Fill(DVP_Image_t *pImage, DVP_S08 *ptr, uint32_t size);

/*!
 * \brief Copies the source image to the destination image with no conversions other than stride.
 * \param [in] pDst The destination image.
 * \param [in] pSrc The source image.
 * \ingroup group_images
 */
DVP_BOOL DVP_Image_Copy(DVP_Image_t *pDst, DVP_Image_t *pSrc);

/*!
 * \brief Duplicates the source DVP_Image_t structure into another structure. No image data is copied, only metadata.
 * \param [in] dst The destintation image metadata structure.
 * \param [in] src The source image metadata structure.
 * \ingroup group_images
 */
void DVP_Image_Dup(DVP_Image_t* dst, DVP_Image_t *src);

/*!
 * \brief Serializes the meta data about the image and the image itself into a flat buffer.
 * \param [in] pImage The image to serialize.
 * \param [in] buffer The buffer to serialize into.
 * \param [in] len The length of the buffer to prevent overflows.
 * \ingroup group_images
 */
size_t DVP_Image_Serialize(DVP_Image_t *pImage, uint8_t *buffer, size_t len);

/*!
 * \brief Unserializes the meta data and the image from a flat buffer.
 * \param [in] pImage The image to unserialize.
 * \param [in] buffer The buffer to unserialize from.
 * \param [in] len The length of the buffer to prevent overflows.
 * \ingroup group_images
 */
size_t DVP_Image_Unserialize(DVP_Image_t *pImage, uint8_t *buffer, size_t len);

/*!
 * \brief Calculates the size of the plane indexed as given.
 * \param [in] pImage The image pointer to calculate the plane size of.
 * \param [in] plane The plane index to use to get the size from.
 * \ingroup group_images
 */
DVP_U32 DVP_Image_PlaneSize(DVP_Image_t *pImage, DVP_U32 plane);

/*!
 * \brief Calculates the byte size of all the data in the image.
 * \param [in] pImage The pointer to the image structure.
 * \ingroup group_images
 */
DVP_U32 DVP_Image_Size(DVP_Image_t *pImage);

/*!
 * \brief Allocates and maps a flat buffer to all remote cores.
 * \param [in] handle The handle to DVP.
 * \param [in] pBuffer The pointer to the buffer structure to fill in.
 * \param [in] dvpMemType The memory type requested.
 * \ingroup group_buffers
 */
DVP_BOOL DVP_Buffer_Alloc(DVP_Handle handle, DVP_Buffer_t *pBuffer, DVP_MemType_e dvpMemType);

/*!
 * \brief Unmaps and Frees the allocated buffer.
 * \param [in] handle The handle to DVP.
 * \param [in] pBuffer the pointer to the buffer structure to free.
 * \ingroup group_buffers
 */
DVP_BOOL DVP_Buffer_Free(DVP_Handle handle, DVP_Buffer_t *pBuffer);

/*!
 * \brief Performs the necessary operations to share this buffer with another process
 * \param [in] handle The handle to DVP.
 * \param [in] pBuffer The pointer to the buffer structure whose data will be shared.
 * \param [in] shared_fd the file descriptor that can be used to import this buffer in a different process
 * \ingroup group_buffers
 */
DVP_BOOL DVP_Buffer_Share(DVP_Handle handle, DVP_Buffer_t *pBuffer, DVP_S32 *shared_fd);

/*!
 * \brief Import the buffer into the calling process
 * \param [in] handle The handle to DVP.
 * \param [in] pBuffer The pointer to the buffer structure whose data will be imported.
 * \param [in] shared_fd the file descriptor obtained from DVP_Buffer_Share
 * \param [in] hdl internal buffer handle, needed to free the image through DVP_Buffer_Free_Import
 * \note This buffer can only be freed through DVP_Buffer_Free_Import
 * \ingroup group_buffers
 */
DVP_BOOL DVP_Buffer_Import(DVP_Handle handle, DVP_Buffer_t *pBuffer, DVP_S32 shared_fd, DVP_VALUE *hdl);

/*!
 * \brief Free an imported buffer
 * \param [in] handle The handle to DVP.
 * \param [in] pBuffer The pointer to the buffer structure whose data will be freed.
 * \param [in] hdl the internal buffer handle obtained during DVP_Buffer_Import
 * \note only imported buffers can be freed through this API
 * \ingroup group_buffers
 */
DVP_BOOL DVP_Buffer_Free_Import(DVP_Handle handle, DVP_Buffer_t *pBuffer, DVP_VALUE hdl);

/*!
 * \brief Initializes the buffer structure to the correct parameters.
 * \note Does not allocate any memory!
 * \param [in] pBuffer The pointer to the structure to be initialized.
 * \param [in] elemSize The size of an individual element.
 * \param [in] numBytes The total number of bytes to be requested.
 * \ingroup group_buffers
 */
void DVP_Buffer_Init(DVP_Buffer_t *pBuffer, DVP_U32 elemSize, DVP_U32 numBytes);

/*!
 * \brief Deinitializes the image structure.
 * \param [in] pBuffer the data structure to clear.
 * \ingroup group_buffers
 */
void DVP_Buffer_Deinit(DVP_Buffer_t *pBuffer);

/*!
 * \brief The function allocates the requested number of DVP_KernelNode_t structures in shared memory and maps them to all enabled remote cores.
 * \param [in] handle The handle to DVP.
 * \param [in] numNodes The desired number of contigiuous node structures.
 * \return DVP_KernelNode_t Returns a mapped array of Nodes.
 * \note The nodes will be allocated with \ref DVP_MTYPE_KERNELGRAPH.
 * \ingroup group_nodes
 */
DVP_KernelNode_t *DVP_KernelNode_Alloc(DVP_Handle handle, DVP_U32 numNodes);

/*!
 * \brief Unmaps and Frees the supplied node array for the number of nodes requested.
 * \param [in] handle The handle to DVP.
 * \param [in] pNodes The array of nodes.
 * \param [in] numNodes The number of nodes that pNodes points to.
 * \ingroup group_nodes
 */
void DVP_KernelNode_Free(DVP_Handle handle, DVP_KernelNode_t *pNodes, DVP_U32 numNodes);

#ifdef __cplusplus
}
#endif

#endif

