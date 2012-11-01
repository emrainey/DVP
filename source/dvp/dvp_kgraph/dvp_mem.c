/**
 *  Copyright (C) 2009-2011 Texas Instruments, Inc.
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

#include <sosal/sosal.h>
#include <dvp/dvp.h>
#include <dvp/dvp_debug.h>
#include <dvp_kgb.h>

// dims mutex_t
static mutex_t dims_mutex = MUTEX_INITIAL;

void dvp_free(DVP_Handle handle, DVP_PTR ptr, DVP_U32 numElem, DVP_U32 sizeElem)
{
    DVP_Dim_t dims[1] = { {{{sizeElem, numElem, 1}}} };
    dvp_mem_free(handle, DVP_MTYPE_DEFAULT, dimof(dims), 2, dims, &ptr);
}

DVP_PTR dvp_malloc(DVP_Handle handle, DVP_U32 size)
{
    DVP_Dim_t dims[1] = { {{{size, 1, 1}}} };
    DVP_PTR ptrs[1] = {NULL};
    DVP_Dim_t strs[1] = { {{{0,0,0}}} };
    DVP_MemType_e mtype = DVP_MTYPE_DEFAULT; // The default type per platform is defined in the dvp_types.h
    dvp_mem_calloc(handle, mtype, dimof(ptrs), 1, dims, ptrs, strs);
    return ptrs[0];
}

DVP_PTR dvp_calloc(DVP_Handle handle, DVP_U32 numElem, DVP_U32 sizeElem)
{
    DVP_Dim_t dims[1] = { {{{sizeElem, numElem, 1}}} };
    DVP_PTR ptrs[1] = {NULL};
    DVP_Dim_t strs[1] = { {{{0,0,0}}} };
    DVP_MemType_e mtype = DVP_MTYPE_DEFAULT; // The default type per platform is defined in the dvp_types.h
    dvp_mem_calloc(handle, mtype, dimof(ptrs), 2, dims, ptrs, strs);
    return ptrs[0];
}

DVP_BOOL DVP_Image_Validate(DVP_Image_t *pImage,
                            DVP_U32 bufAlign,
                            DVP_U32 strideMultiple,
                            DVP_U32 widthMultiple,
                            DVP_U32 heigthMultiple,
                            fourcc_t *colors,
                            DVP_U32 numColors)
{
    DVP_U32 p;
    if (pImage == NULL)
        return DVP_FALSE;

    if (bufAlign == 0 ||
        strideMultiple == 0 ||
        widthMultiple == 0 ||
        heigthMultiple == 0)
        return DVP_FALSE;

    if (pImage->planes == 0 ||
        pImage->width == 0 ||
        pImage->height == 0 ||
        pImage->bufHeight == 0 ||
        pImage->bufWidth == 0 ||
        pImage->color == 0 ||
        pImage->memType < DVP_MTYPE_MPUCACHED_VIRTUAL ||
        pImage->memType > DVP_MTYPE_MAX ||
        pImage->numBytes == 0 ||
        pImage->x_stride == 0 ||
        pImage->y_stride == 0)
        return DVP_FALSE;

    if ((pImage->y_stride % strideMultiple) ||
        (pImage->width % widthMultiple) ||
        (pImage->height % heigthMultiple))
        return DVP_FALSE;

    for (p = 0; p < pImage->planes; p++)
    {
        size_t range = abs(pImage->y_stride) * pImage->bufHeight;
        size_t line = pImage->width * pImage->x_stride;
        DVP_U08 *pLo = pImage->pBuffer[p];
        DVP_U08 *pHi = pImage->pBuffer[p] + range;

        // is the pointer NULL for either?
        if (pImage->pBuffer[p] == NULL || pImage->pData[p] == NULL)
            return DVP_FALSE;

        // if the pointer not aligned?
        if ((size_t)pImage->pData[p] % bufAlign)
            return DVP_FALSE;

        // check to make sure the pointer is within the image somewhere
        if (!(pLo <= pImage->pData[p] && pImage->pData[p] < pHi))
            return DVP_FALSE;

        // if the image is strided, check to make sure it's not in the badlands.
        if (abs(pImage->y_stride) > line && pImage->bufWidth == pImage->width)
        {
            size_t offset = (size_t)(pImage->pData[p] - pImage->pBuffer[p]);
            if ((offset % abs(pImage->y_stride)) > line)
                return DVP_FALSE;
        }
    }

    if (numColors > 0 && colors != NULL)
    {
        DVP_U32 c;
        DVP_BOOL colorMatch = DVP_FALSE;
        for (c = 0; c < numColors; c++)
            if (colors[c] == pImage->color)
                colorMatch = DVP_TRUE;
        if (colorMatch == DVP_FALSE)
            return DVP_FALSE;
    }
    return DVP_TRUE;
}

void DVP_Image_Dup(DVP_Image_t *dst, DVP_Image_t *src)
{
    memcpy(dst, src, sizeof(DVP_Image_t));
}

DVP_U32 DVP_Image_PlaneSize(DVP_Image_t *pImage, DVP_U32 plane)
{
    DVP_U32 lineSize = 0;
    DVP_U32 planeSize = 0;

    if (pImage == NULL)
        return 0;

    lineSize = pImage->x_stride * pImage->bufWidth;
    planeSize = pImage->y_stride * pImage->bufHeight; // potentially overestimated

    if (plane > 0)
    {
        switch (pImage->color)
        {
            case FOURCC_NV12:
            case FOURCC_NV21:
                // lineSize == lineSize // due to macropixel * w/2
                planeSize = lineSize * pImage->bufHeight/2;
                break;
            case FOURCC_YU16:
            case FOURCC_YV16:
                // lineSize == lineSize // due to macropixel * w/2
                planeSize = lineSize * pImage->bufHeight;
                break;
            case FOURCC_YV12:
            case FOURCC_IYUV:
                if (lineSize == (DVP_U32)abs(pImage->y_stride)) // non-TILED
                    lineSize /= 2;
                planeSize = lineSize * pImage->bufHeight/2;
                break;
            case FOURCC_YUV9:
            case FOURCC_YVU9:
                if (lineSize == (DVP_U32)abs(pImage->y_stride)) // non-TILED
                    lineSize /= 4;
                planeSize = lineSize * pImage->bufHeight/4;
                break;
            default:
                break;
        }
    }
    return planeSize;
}

DVP_U32 DVP_Image_Size(DVP_Image_t *pImage)
{
    DVP_U32 p,size = 0;
    for (p = 0; p < pImage->planes; p++)
        size += DVP_Image_PlaneSize(pImage, p);
    return size;
}

DVP_Dim_t *DVP_Image_Dims(DVP_Image_t *pImage)
{
    static DVP_Dim_t dims[DVP_MAX_PLANES];

    memset(dims, 0, sizeof(dims));

    switch (pImage->color)
    {
        // semi-planar
        case FOURCC_NV12:
        case FOURCC_NV21:
            dims[0].img.bpp    = 1;
            dims[0].img.width  = pImage->bufWidth;
            dims[0].img.height = pImage->bufHeight;
            dims[1].img.bpp    = dims[0].img.bpp*2;
            dims[1].img.width  = dims[0].img.width/2;
            dims[1].img.height = dims[0].img.height/2;
            break;
        case FOURCC_YUV9:
        case FOURCC_YVU9:
            dims[0].img.bpp    = 1;
            dims[0].img.width  = pImage->bufWidth;
            dims[0].img.height = pImage->bufHeight;
            dims[1].img.bpp    = dims[2].img.bpp    = dims[0].img.bpp;
            dims[1].img.width  = dims[2].img.width  = dims[0].img.width/4;
            dims[1].img.height = dims[2].img.height = dims[0].img.height/4;
            break;
        case FOURCC_YV12:
        case FOURCC_IYUV:
            dims[0].img.bpp    = 1;
            dims[0].img.width  = pImage->bufWidth;
            dims[0].img.height = pImage->bufHeight;
            dims[1].img.bpp    = dims[2].img.bpp    = dims[0].img.bpp;
            dims[1].img.width  = dims[2].img.width  = dims[0].img.width/2;
            dims[1].img.height = dims[2].img.height = dims[0].img.height/2;
            break;
        case FOURCC_YU16:
        case FOURCC_YV16:
            dims[0].img.bpp    = 1;
            dims[0].img.width  = pImage->bufWidth;
            dims[0].img.height = pImage->bufHeight;
            dims[1].img.bpp    = dims[2].img.bpp    = dims[0].img.bpp*2;
            dims[1].img.width  = dims[2].img.width  = dims[0].img.width/2;
            dims[1].img.height = dims[2].img.height = dims[0].img.height;
            break;
#if defined(DVP_USE_TILER)
        case FOURCC_UYVY:
        case FOURCC_YUY2:
        case FOURCC_VYUY:
        case FOURCC_YVYU:
            dims[0].img.bpp = 4;
            dims[0].img.width = pImage->bufWidth/2;
            dims[0].img.height = pImage->bufHeight;
            break;
#endif
        // iso-planar (includes single plane)
        default:
        {
            DVP_U32 p = 0;
            for (p = 0; p < pImage->planes; p++) {
                dims[p].img.bpp = pImage->x_stride;
                dims[p].img.width = pImage->bufWidth;
                dims[p].img.height = pImage->bufHeight;
            }
            break;
        }
    }
    return dims;
}

DVP_BOOL DVP_Image_Free(DVP_Handle handle, DVP_Image_t *pImage)
{
    DVP_BOOL ret = DVP_FALSE;
#if defined(DVP_USE_CAMERA_SERVICE)
    if (handle && pImage && pImage->memType != DVP_MTYPE_DISPLAY_2DTILED && pImage->memType != DVP_MTYPE_CAMERA_1DTILED)
#else
    if (handle && pImage && pImage->memType != DVP_MTYPE_DISPLAY_2DTILED)
#endif
    {
        DVP_Dim_t *dims = NULL;
        DVP_U32 ndims = 3;
        DVP_U32 nptrs = pImage->planes;

        mutex_lock(&dims_mutex);
        dims = DVP_Image_Dims(pImage);
#if defined(DVP_USE_TILER)
        nptrs = 1;  // tiler buffers are allocated in bulk but freed alone
#endif
#if defined(DVP_USE_BO)
        if (pImage->memType == DVP_MTYPE_GRALLOC_2DTILED ||
            pImage->memType == DVP_MTYPE_MPUNONCACHED_2DTILED)
            free(pImage->reserved); // free the array of pointers to omap_bo's
#endif
        ret = dvp_mem_free(handle, pImage->memType, nptrs, ndims, dims, (DVP_PTR *)pImage->pBuffer);
        mutex_unlock(&dims_mutex);
    }
    else if (handle && pImage && pImage->memType == DVP_MTYPE_DISPLAY_2DTILED)
    {
        // dissociate the display buffer with the remote cores
        DVP_t *dvp = (DVP_t *)handle;
        DVP_U32 nptrs = pImage->planes;
        DVP_U32 p = 0;
        DVP_VALUE hdls[DVP_MAX_PLANES] = {0, 0, 0, 0};
#if defined(DVP_USE_TILER) || defined(GRALLOC_USE_MULTIPLE_POINTERS)
        if (pImage->color == FOURCC_NV12)
            nptrs = 1;
#endif
        for (p = 0; p < nptrs; p++)
        {
            ret = dvp_rpc_dissociate(dvp->rpc, dvp->mem, pImage->pBuffer[p], &hdls[p], pImage->numBytes, pImage->memType);
            if (ret == DVP_FALSE)
            {
                DVP_PRINT(DVP_ZONE_ERROR, "Failed to dissociate Display buffer %p with remote core!\n", pImage->pBuffer[p]);
            }
        }
    }
#if defined(DVP_USE_CAMERA_SERVICE)
    else if (handle && pImage && pImage->memType == DVP_MTYPE_CAMERA_1DTILED)
    {
        DVP_t *dvp = (DVP_t *)handle;
        ret = dvp_rpc_dissociate(dvp->rpc, dvp->mem, pImage->pBuffer[0], pImage->reserved, pImage->numBytes, pImage->memType);
        if (ret == DVP_FALSE)
        {
            DVP_PRINT(DVP_ZONE_ERROR, "Failed to dissociate Camera buffer %p with remote core!\n", pImage->pBuffer[0]);
        }
    }
#endif
    return ret;
}

DVP_BOOL DVP_Image_Alloc(DVP_Handle handle, DVP_Image_t *pImage, DVP_MemType_e dvpMemType)
{
    DVP_BOOL ret = DVP_FALSE;
#if defined(DVP_USE_CAMERA_SERVICE)
    if (handle && pImage && dvpMemType != DVP_MTYPE_DISPLAY_2DTILED && dvpMemType != DVP_MTYPE_CAMERA_1DTILED)
#else
    if (handle && pImage && dvpMemType != DVP_MTYPE_DISPLAY_2DTILED)
#endif
    {
        DVP_U32 ndims = 3;
        DVP_U32 nptrs = pImage->planes;
        DVP_U32 p = 0;
        DVP_Dim_t *dims = NULL;
        DVP_Dim_t strides[DVP_MAX_PLANES] = {{{{0,0,0}}},
                                             {{{0,0,0}}},
                                             {{{0,0,0}}},
                                             {{{0,0,0}}}};

        mutex_lock(&dims_mutex);
        dims = DVP_Image_Dims(pImage);
        if (dvp_mem_calloc(handle, dvpMemType, nptrs, ndims, dims, (DVP_PTR *)pImage->pBuffer, strides) == DVP_TRUE)
        {
            for (p = 0; p < pImage->planes; p++)
                pImage->pData[p] = pImage->pBuffer[p]; // assign each pointer

            pImage->memType = dvpMemType;
            // just take the first plane's strides since DVP_Image_t does not track each plane.
#if defined(DVP_USE_TILER)
            if (pImage->color == FOURCC_UYVY || pImage->color == FOURCC_YUY2 ||
                pImage->color == FOURCC_VYUY || pImage->color == FOURCC_YVYU)
            {
                strides[0].dim.x = 2; // was set to 4 to force 32 TILER allocations
            }
#endif
            pImage->x_stride = strides[0].dim.x;
            pImage->y_stride = strides[0].dim.y;
            pImage->numBytes = strides[0].dim.z;
#if defined(DVP_USE_GRALLOC)
            if (pImage->memType == DVP_MTYPE_GRALLOC_2DTILED)
            {
                // these will probably be used by the camera, so they need to have the reserved field set to the pointer to the gralloc data
                DVP_t *dvp = (DVP_t *)handle;
                pImage->reserved = (DVP_PTR)dvp_rpc_get_handle(dvp->rpc, pImage->pBuffer[0]);
                if (pImage->pBuffer[1] == NULL)
                {
                    pImage->pData[1] = pImage->pBuffer[1] = &pImage->pBuffer[0][pImage->y_stride*pImage->bufHeight];
                }
            }
#elif defined(DVP_USE_BO)
            if (pImage->memType == DVP_MTYPE_GRALLOC_2DTILED ||  // i regret naming it this way now...
                pImage->memType == DVP_MTYPE_MPUNONCACHED_2DTILED)
            {
                DVP_t *dvp = (DVP_t *)handle;
                // allocate an array of bo pointers for the
                alloc_omap_bo_t **bos = (alloc_omap_bo_t *)calloc(pImage->planes, sizeof(alloc_omap_bo_t *));
                if (bos)
                {
                    DVP_U32 p = 0;
                    for (p = 0; p < pImage->planes; p++)
                        bos[p] = (alloc_omap_bo_t *)dvp_rpc_get_handle(dvp->rpc, pImage->pBuffer[p]);
                    pImage->reserved = (DVP_PTR)bos;
                }
            }
#endif
            ret = DVP_TRUE;
        }
        mutex_unlock(&dims_mutex);
    }
    else if (handle && pImage && dvpMemType == DVP_MTYPE_DISPLAY_2DTILED)
    {
        // display buffers need to be associated with the remote cores, only UYVY or NV12 is supported
        DVP_t *dvp = (DVP_t *)handle;
        DVP_U32 planeSize = 0;
        if (pImage->color == FOURCC_UYVY) // TILER
        {
            ret = dvp_rpc_associate(dvp->rpc, dvp->mem, pImage->pBuffer[0], (DVP_VALUE)pImage->reserved, pImage->numBytes, dvpMemType);
            if (ret == DVP_FALSE)
            {
                DVP_PRINT(DVP_ZONE_ERROR, "Failed to associate Display buffer %p with remote core!\n", pImage->pBuffer[0]);
            }
        }
        else if (pImage->color == FOURCC_NV12) // TILER/ION
        {
#if !defined(DVP_USE_TILER) && defined(GRALLOC_USE_MULTIPLE_POINTERS)
            planeSize = DVP_Image_PlaneSize(pImage, 0);
            ret = dvp_rpc_associate(dvp->rpc, dvp->mem, pImage->pBuffer[0], (DVP_VALUE)pImage->reserved, planeSize, dvpMemType);
            if (ret == DVP_FALSE)
            {
                DVP_PRINT(DVP_ZONE_ERROR, "Failed to associate Display buffer %p with remote core!\n", pImage->pBuffer[0]);
            }
            planeSize = DVP_Image_PlaneSize(pImage, 1);
            ret = dvp_rpc_associate(dvp->rpc, dvp->mem, pImage->pBuffer[1], (DVP_VALUE)NULL, planeSize, dvpMemType);
            if (ret == DVP_FALSE)
            {
                DVP_PRINT(DVP_ZONE_ERROR, "Failed to associate Display buffer %p with remote core!\n", pImage->pBuffer[1]);
            }
#else
            planeSize = DVP_Image_PlaneSize(pImage,0) + DVP_Image_PlaneSize(pImage, 1);
            ret = dvp_rpc_associate(dvp->rpc, dvp->mem, pImage->pBuffer[0], (DVP_VALUE)pImage->reserved, planeSize, dvpMemType);
            if (ret == DVP_FALSE)
            {
                DVP_PRINT(DVP_ZONE_ERROR, "Failed to associate Display buffer %p with remote core!\n", pImage->pBuffer[0]);
            }
#endif
        }
    }
#if defined(DVP_USE_CAMERA_SERVICE)
    else if (handle && pImage && dvpMemType == DVP_MTYPE_CAMERA_1DTILED)
    {
        DVP_t *dvp = (DVP_t *)handle;
        DVP_U32 p = 0, planeSize = 0;

        pImage->reserved = pImage->pBuffer[0];

        for (p = 0; p < pImage->planes; p++)
            pImage->pData[p] = pImage->pBuffer[p]; // assign each pointer

        pImage->memType = dvpMemType;
        pImage->y_stride = pImage->bufWidth * pImage->x_stride;

        ret = dvp_rpc_associate(dvp->rpc, dvp->mem, pImage->pBuffer[0], (DVP_VALUE)pImage->reserved, pImage->numBytes, dvpMemType);
        if (ret == DVP_FALSE)
        {
            DVP_PRINT(DVP_ZONE_ERROR, "Failed to associate Camera buffer %p with remote core!\n", pImage->pBuffer[0]);
        }
    }
#endif
    DVP_PrintImage(DVP_ZONE_MEM, pImage);
    return ret;
}

DVP_BOOL DVP_Image_Share(DVP_Handle handle, DVP_Image_t *pImage, DVP_S32 *fds)
{
    DVP_BOOL ret = DVP_FALSE;
    if (handle && pImage && pImage->memType != DVP_MTYPE_DISPLAY_2DTILED)
    {
        DVP_U32 nptrs = pImage->planes;
        ret = dvp_mem_share(handle, pImage->memType, nptrs, (DVP_PTR *)pImage->pBuffer, fds);
    }
    return ret;
}

DVP_BOOL DVP_Image_Import(DVP_Handle handle, DVP_Image_t *pImage, DVP_S32 *shared_fds, DVP_VALUE *hdls)
{
    DVP_BOOL ret = DVP_FALSE;
    if (handle && pImage && pImage->memType != DVP_MTYPE_DISPLAY_2DTILED)
    {
        DVP_Dim_t *dims = NULL;
        DVP_U32 ndims = 3;
        DVP_U32 nptrs = pImage->planes;
        DVP_Dim_t strides[DVP_MAX_PLANES] = {{{{0,0,0}}},
                                             {{{0,0,0}}},
                                             {{{0,0,0}}},
                                             {{{0,0,0}}}};
        mutex_lock(&dims_mutex);
        dims = DVP_Image_Dims(pImage);
#if defined(DVP_USE_TILER)
        nptrs = 1;  // tiler buffers are allocated in bulk but freed alone
#endif
        if (dvp_mem_import(handle, pImage->memType, nptrs, ndims, dims, (DVP_PTR *)pImage->pBuffer, strides, shared_fds, hdls) == DVP_TRUE)
        {
            DVP_U32 p;
#if defined(DVP_USE_TILER)
            if (pImage->color == FOURCC_UYVY || pImage->color == FOURCC_YUY2 ||
                pImage->color == FOURCC_VYUY || pImage->color == FOURCC_YVYU)
            {
                strides[0].dim.x = 2; // was set to 4 to force 32 TILER allocations
            }
#endif
            for (p = 0; p < pImage->planes; p++)
                pImage->pData[p] = pImage->pBuffer[p]; // assign each pointer
            pImage->x_stride = strides[0].dim.x;
            pImage->y_stride = strides[0].dim.y;
            pImage->numBytes = strides[0].dim.z;
            ret = DVP_TRUE;
        }
        mutex_unlock(&dims_mutex);
    }
    return ret;
}

DVP_BOOL DVP_Image_Free_Import(DVP_Handle handle, DVP_Image_t *pImage, DVP_VALUE *hdls)
{
    DVP_BOOL ret = DVP_FALSE;
    if (handle && pImage && pImage->memType != DVP_MTYPE_DISPLAY_2DTILED)
    {
        DVP_U32 ndims = 3;
        DVP_Dim_t *dims = NULL;
        DVP_U32 nptrs = pImage->planes;
        mutex_lock(&dims_mutex);
        dims = DVP_Image_Dims(pImage);
#if defined(DVP_USE_TILER)
        nptrs = 1;  // tiler buffers are allocated in bulk but freed alone
#endif
        ret = dvp_mem_free_import(handle, pImage->memType, ndims, dims, nptrs, (DVP_PTR *)pImage->pBuffer, hdls);
        mutex_unlock(&dims_mutex);
    }
    return ret;
}

void DVP_Image_Fill(DVP_Image_t *pImage, DVP_S08 *ptr, uint32_t size)
{
    if (size >= (pImage->height*pImage->width*pImage->x_stride))
    {
        uint32_t i,y,l;
        for (y = 0; y < pImage->height; y++)
        {
            i = (y * pImage->y_stride);
            l = (pImage->width * pImage->x_stride);
            memcpy(&pImage->pData[0][i], &ptr[y*l], l);
        }
    }
}

DVP_BOOL DVP_Image_Copy(DVP_Image_t *pDst, DVP_Image_t *pSrc)
{
    DVP_U32 p,x,y,i,j,l;
    DVP_PrintImage(DVP_ZONE_MEM, pSrc);
    DVP_PrintImage(DVP_ZONE_MEM, pDst);

    if (pSrc == NULL || pDst == NULL)
        return DVP_FALSE;

    for (p = 0; p < pSrc->planes; p++)
        if (pSrc->pData[p] == NULL)
            return DVP_FALSE;

    for (p = 0; p < pDst->planes; p++)
        if (pDst->pData[p] == NULL)
            return DVP_FALSE;

    if (pSrc->width > pDst->width ||
        pSrc->height > pDst->height)
        return DVP_FALSE;

    if (pDst->color == pSrc->color)
    {
        DVP_U32 xscale[DVP_MAX_PLANES] = {1,1,1,1};
        DVP_U32 yscale[DVP_MAX_PLANES] = {1,1,1,1};

        switch (pSrc->color)
        {
            case FOURCC_IYUV: // YUV420 planar formats
            case FOURCC_YV12:
                xscale[1] = yscale[1] = 2;
                xscale[2] = yscale[2] = 2;
                break;
            case FOURCC_YVU9: // YUV411 planar formats
            case FOURCC_YUV9:
                xscale[1] = yscale[1] = 4;
                xscale[2] = yscale[2] = 4;
                break;
            case FOURCC_YV16: // YUV422 planar formats
            case FOURCC_YU16:
                xscale[1] = xscale[2] = 2;
                yscale[1] = yscale[2] = 1;
                break;
            case FOURCC_NV12:
            case FOURCC_NV21:
                // xscale is literally 2 but macro pixel makes it equivalent to 1.
                yscale[1] = 2;
                break;
            case FOURCC_YV24: // 4:4:4 planar formats
            case FOURCC_YU24:
            case FOURCC_RGBP:
            case FOURCC_RGB565:
            default:
                // default values are fine
                break;
        }

        for (p = 0; p < pSrc->planes; p++)
        {
            for (y = 0; y < (pSrc->height/yscale[p]); y++)
            {
                i = (y * pSrc->y_stride);
                j = (y * pDst->y_stride);
                l = (pSrc->width * pSrc->x_stride)/xscale[p];
                memcpy(&pDst->pData[p][j], &pSrc->pData[p][i], l);
            }
        }
        return DVP_TRUE;
    }
    else
    {
        // convert the colors
        if (pSrc->color == FOURCC_Y800 && (pDst->color == FOURCC_UYVY || pDst->color == FOURCC_VYUY))
        {
            DVP_U08 *luma;
            DVP_U08 *xyxy;
            for (y = pSrc->y_start; y < pSrc->height; y++)
            {
                for (x = pSrc->x_start; x < pSrc->width; x+=2) // macro pixel
                {
                    i = (y * pSrc->y_stride) + (x * pSrc->x_stride);
                    j = (y * pDst->y_stride) + (x * pDst->x_stride);
                    luma = &pSrc->pData[0][i];
                    xyxy = &pDst->pData[0][j];
                    xyxy[0] = 128; // "zero" Chromance
                    xyxy[1] = luma[0];
                    xyxy[2] = 128; // "zero" Chromance
                    xyxy[3] = luma[1];
                }
            }
            return DVP_TRUE;
        }
        if (pSrc->color == FOURCC_Y800 && (pDst->color == FOURCC_YUY2 || pDst->color == FOURCC_YVYU))
        {
            DVP_U08 *luma;
            DVP_U08 *yxyx;
            for (y = pSrc->y_start; y < pSrc->height; y++)
            {
                for (x = pSrc->x_start; x < pSrc->width; x+=2) // macro pixel
                {
                    i = (y * pSrc->y_stride) + (x * pSrc->x_stride);
                    j = (y * pDst->y_stride) + (x * pDst->x_stride);
                    luma = &pSrc->pData[0][i];
                    yxyx = &pDst->pData[0][j];
                    yxyx[0] = luma[0];
                    yxyx[1] = 128; // "zero" Chromance
                    yxyx[2] = luma[1];
                    yxyx[3] = 128; // "zero" Chromance
                }
            }
            return DVP_TRUE;
        }
        else if ((pDst->color == FOURCC_UYVY || pDst->color == FOURCC_VYUY) &&
                 (pSrc->color == FOURCC_YVYU || pSrc->color == FOURCC_YUY2))
        {
            // reswizzle
            DVP_U08 *xyxy;
            DVP_U08 *yxyx;
            for (y = pSrc->y_start; y < pSrc->height; y++)
            {
                for (x = pSrc->x_start; x < pSrc->width; x+=2) // macro pixels
                {
                    i = (y * pSrc->y_stride) + (x * pSrc->x_stride);
                    j = (y * pDst->y_stride) + (x * pDst->x_stride);
                    yxyx = &pSrc->pData[0][i];
                    xyxy = &pDst->pData[0][j];
                    if (pSrc->color == FOURCC_YUY2)
                    {
                        if (pDst->color == FOURCC_UYVY)
                        {
                          //UYVY      YUYV
                            xyxy[0] = yxyx[1];
                            xyxy[1] = yxyx[0];
                            xyxy[2] = yxyx[3];
                            xyxy[3] = yxyx[2];
                        }
                        else
                        {
                          //VYUY      YUYV
                            xyxy[0] = yxyx[3];
                            xyxy[1] = yxyx[0];
                            xyxy[2] = yxyx[1];
                            xyxy[3] = yxyx[2];
                        }
                    }
                    else
                    {
                        if (pDst->color == FOURCC_UYVY)
                        {
                          //UYVY      YVYU
                            xyxy[0] = yxyx[3];
                            xyxy[1] = yxyx[0];
                            xyxy[2] = yxyx[1];
                            xyxy[3] = yxyx[2];
                        }
                        else
                        {
                          //VYUY      YVYU
                            xyxy[0] = yxyx[1];
                            xyxy[1] = yxyx[0];
                            xyxy[2] = yxyx[3];
                            xyxy[3] = yxyx[2];
                        }
                    }
                }
            }
            return DVP_TRUE;
        }
        // some semi-planar or planar
        else if ((pSrc->color == FOURCC_NV12 ||
                  pSrc->color == FOURCC_YU24 ||
                  pSrc->color == FOURCC_YV24 ||
                  pSrc->color == FOURCC_YU16 ||
                  pSrc->color == FOURCC_YV16 ||
                  pSrc->color == FOURCC_IYUV ||
                  pSrc->color == FOURCC_YV12 ||
                  pSrc->color == FOURCC_YUV9 ||
                  pSrc->color == FOURCC_YVU9) &&
                 pDst->color == FOURCC_Y800)
        {
            // a "luma" extract kind-of
            for (y = pSrc->y_start; y < pSrc->height; y++)
            {
                uint32_t i = (y * pSrc->y_stride);
                uint32_t j = (y * pDst->y_stride);
                uint32_t l = (pSrc->x_stride * pSrc->width);
                memcpy(&pDst->pData[0][j], &pSrc->pData[0][i], l);
            }
            return DVP_TRUE;
        }
    }
    return DVP_FALSE;
}

void DVP_Image_Init(DVP_Image_t *pImage, DVP_U32 width, DVP_U32 height, fourcc_t fourcc)
{
    // clean out any dirty values
    memset(pImage, 0, sizeof(DVP_Image_t));

    pImage->planes = 1;   // assume single plane
    pImage->x_stride = 1; // default size

    if (fourcc == FOURCC_YUV9 ||
        fourcc == FOURCC_YVU9 ||
        fourcc == FOURCC_YV12 ||
        fourcc == FOURCC_IYUV ||
        fourcc == FOURCC_YU16 ||
        fourcc == FOURCC_YV16 ||
        fourcc == FOURCC_YU24 ||
        fourcc == FOURCC_YV24 ||
        fourcc == FOURCC_RGBP ||
        fourcc == FOURCC_HSLP)
        pImage->planes = 3;

    if (fourcc == FOURCC_UYVY ||
        fourcc == FOURCC_YUY2 ||
        fourcc == FOURCC_VYUY ||
        fourcc == FOURCC_YVYU)
        pImage->x_stride = 2;

    if (fourcc == FOURCC_BGR ||
        fourcc == FOURCC_RGB)
        pImage->x_stride = 3;

    if (fourcc == FOURCC_RGBA ||
        fourcc == FOURCC_BGRA ||
        fourcc == FOURCC_ARGB)
        pImage->x_stride = 4;

    if (fourcc == FOURCC_NV12 ||
        fourcc == FOURCC_NV21)
        pImage->planes = 2;

    if (fourcc == FOURCC_Y16)
        pImage->x_stride = 2;

    if (fourcc == FOURCC_Y32)
        pImage->x_stride = 4;

    if (fourcc == FOURCC_RGB565 ||
        fourcc == FOURCC_BGR565)
        pImage->x_stride = 2;

    // calculate the number of bytes (assumes isometric planes)
    pImage->numBytes = height * width * pImage->x_stride * pImage->planes;

    // 4:2:0 sizes are different
    if (fourcc == FOURCC_NV12 ||
        fourcc == FOURCC_NV21 ||
        fourcc == FOURCC_IYUV ||
        fourcc == FOURCC_YV12)
        pImage->numBytes = (height * width * 3)/2;

    // 4:2:2 sizes are also different
    if (fourcc == FOURCC_YV16 ||
        fourcc == FOURCC_YU16)
        pImage->numBytes = (height * width) + 2*(height * width/2);

    pImage->width = width;
    pImage->height = height;
    pImage->bufWidth = width;
    pImage->bufHeight = height;
    pImage->color = fourcc;
}

void DVP_Image_Deinit(DVP_Image_t *pImage)
{
    memset(pImage, 0xEF, sizeof(DVP_Image_t));
}

size_t DVP_Image_Unserialize(DVP_Image_t *pImage, uint8_t *buffer, size_t len)
{
    size_t offset = 0;
    uint32_t p,y,i,l;
    DVP_Dim_t *dims = NULL;
    DVP_Image_t image;
    bool_e copy = false_e;

    UNSERIALIZE_UNIT(buffer, offset, len, image.planes);
    UNSERIALIZE_UNIT(buffer, offset, len, image.width);
    UNSERIALIZE_UNIT(buffer, offset, len, image.height);
    UNSERIALIZE_UNIT(buffer, offset, len, image.bufWidth);
    UNSERIALIZE_UNIT(buffer, offset, len, image.bufHeight);
    UNSERIALIZE_UNIT(buffer, offset, len, image.x_start);
    UNSERIALIZE_UNIT(buffer, offset, len, image.y_start);
    //UNSERIALIZE_UNIT(buffer, offset, len, image.x_stride);
    //UNSERIALIZE_UNIT(buffer, offset, len, image.y_stride);
    UNSERIALIZE_UNIT(buffer, offset, len, image.color);
    UNSERIALIZE_UNIT(buffer, offset, len, image.numBytes);
    //UNSERIALIZE_UNIT(buffer, offset, len, image.memType);
    //UNSERIALIZE_UNIT(buffer, offset, len, image.skipCacheOpInval);
    //UNSERIALIZE_UNIT(buffer, offset, len, image.skipCacheOpFlush);

    // only copy if the images are a match.
    if (image.color == pImage->color &&
        image.bufWidth == pImage->bufWidth &&
        image.bufHeight == pImage->bufHeight)
        copy = true_e;

    mutex_lock(&dims_mutex);
    dims = DVP_Image_Dims(&image);
    for (p = 0; p < image.planes; p++)
    {
        for (y = pImage->y_start; y < (uint32_t)dims[p].img.height; y++)
        {
            // @TODO This should be identical across all planes
            i = (y * pImage->y_stride) + (0 * dims[p].dim.x);
            // @TODO This should be identical across all planes
            l = (pImage->x_stride * dims[p].img.width);
            if (copy) {
                UNSERIALIZE_ARRAY(buffer, offset, len, &pImage->pBuffer[p][i], l);
            } else {
                offset += l;
            }
        }
    }
    mutex_unlock(&dims_mutex);
    return offset;

}

size_t DVP_Image_Serialize(DVP_Image_t *pImage, uint8_t *buffer, size_t len)
{
    size_t offset = 0;
    uint32_t p,y,i,l;
    DVP_Dim_t *dims = NULL;

    SERIALIZE_UNIT(buffer, offset, len, pImage->planes);
    SERIALIZE_UNIT(buffer, offset, len, pImage->width);
    SERIALIZE_UNIT(buffer, offset, len, pImage->height);
    SERIALIZE_UNIT(buffer, offset, len, pImage->bufWidth);
    SERIALIZE_UNIT(buffer, offset, len, pImage->bufHeight);
    SERIALIZE_UNIT(buffer, offset, len, pImage->x_start);
    SERIALIZE_UNIT(buffer, offset, len, pImage->y_start);
    //SERIALIZE_UNIT(buffer, offset, len, pImage->x_stride);
    //SERIALIZE_UNIT(buffer, offset, len, pImage->y_stride);
    SERIALIZE_UNIT(buffer, offset, len, pImage->color);
    SERIALIZE_UNIT(buffer, offset, len, pImage->numBytes);
    //SERIALIZE_UNIT(buffer, offset, len, pImage->memType);
    //SERIALIZE_UNIT(buffer, offset, len, pImage->skipCacheOpInval);
    //SERIALIZE_UNIT(buffer, offset, len, pImage->skipCacheOpFlush);

    mutex_lock(&dims_mutex);
    dims = DVP_Image_Dims(pImage);
    for (p = 0; p < pImage->planes; p++)
    {
        for (y = 0; y < (uint32_t)dims[p].img.height; y++)
        {
            // @TODO This should be identical across all planes
            i = (y * pImage->y_stride) + (0 * dims[p].dim.x);
            // @TODO This should be identical across all planes
            l = (pImage->x_stride * dims[p].img.width);
            SERIALIZE_ARRAY(buffer, offset, len, &pImage->pBuffer[p][i], l);
        }
    }
    mutex_unlock(&dims_mutex);
    return offset;
}

DVP_BOOL DVP_Buffer_Validate(DVP_Buffer_t *pBuffer)
{
    if (pBuffer == NULL)
        return DVP_FALSE;

    if (pBuffer->elemSize == 0 ||
        pBuffer->numBytes == 0 ||
        pBuffer->memType >= DVP_MTYPE_MAX ||
        pBuffer->pData == NULL)
        return DVP_FALSE;
    return DVP_TRUE;
}

DVP_BOOL DVP_Buffer_Free(DVP_Handle handle, DVP_Buffer_t *pBuffer)
{
    if (handle && pBuffer)
    {
        DVP_U32 numElem = pBuffer->numBytes/pBuffer->elemSize;
        DVP_Dim_t dims[1] = { {{{pBuffer->elemSize, numElem, 1}}} };
        return dvp_mem_free(handle, pBuffer->memType, dimof(dims), 2, dims, (DVP_PTR *)&pBuffer->pData);
    }
    return DVP_FALSE;
}

DVP_BOOL DVP_Buffer_Alloc(DVP_Handle handle, DVP_Buffer_t *pBuffer, DVP_MemType_e dvpMemType)
{
    DVP_BOOL ret = DVP_FALSE;
    if (handle && pBuffer)
    {
        DVP_U32 numElem = pBuffer->numBytes/pBuffer->elemSize;
        DVP_Dim_t dims[1] = { {{{pBuffer->elemSize, numElem, 1}}} };
        DVP_PTR ptrs[1] = {NULL};
        DVP_Dim_t strs[1] = { {{{0,0,0}}} };
        ret = dvp_mem_calloc(handle, dvpMemType, dimof(ptrs), 2, dims, ptrs, strs);
        if (ret == DVP_TRUE) {
            pBuffer->memType = dvpMemType;
            pBuffer->pData = (DVP_U08 *)ptrs[0];
        }
    }
    return ret;
}

void DVP_Buffer_Init(DVP_Buffer_t *pBuffer, DVP_U32 elemSize, DVP_U32 numElems)
{
    // clean out any dirty values
    memset(pBuffer, 0, sizeof(DVP_Buffer_t));

    pBuffer->elemSize = elemSize;
    pBuffer->numBytes = elemSize*numElems;
}

void DVP_Buffer_Deinit(DVP_Buffer_t *pBuffer)
{
    memset(pBuffer, 0xEF, sizeof(DVP_Buffer_t));
}

DVP_BOOL DVP_Buffer_Share(DVP_Handle handle, DVP_Buffer_t *pBuffer, DVP_S32 *shared_fd)
{
    DVP_BOOL ret = DVP_FALSE;
    if (handle && pBuffer)
    {
        ret = dvp_mem_share(handle, pBuffer->memType, 1, (DVP_PTR *)&pBuffer->pData, shared_fd);
    }
    return ret;
}

DVP_BOOL DVP_Buffer_Import(DVP_Handle handle, DVP_Buffer_t *pBuffer, DVP_S32 shared_fd, DVP_VALUE *hdl)
{
    DVP_BOOL ret = DVP_FALSE;
    if (handle && pBuffer)
    {
        DVP_U32 numElem = pBuffer->numBytes/pBuffer->elemSize;
        DVP_Dim_t dims[1] = { {{{pBuffer->elemSize, numElem, 1}}} };
        DVP_PTR ptrs[1] = {NULL};
        DVP_Dim_t strs[1] = { {{{0,0,0}}} };
        if (dvp_mem_import(handle, pBuffer->memType, 1, 2, dims, (DVP_PTR *)ptrs, strs, &shared_fd, hdl) == DVP_TRUE)
        {
            pBuffer->pData = (DVP_U08 *)ptrs[0];
            ret = DVP_TRUE;
        }
    }
    return ret;
}

DVP_BOOL DVP_Buffer_Free_Import(DVP_Handle handle, DVP_Buffer_t *pBuffer, DVP_VALUE hdl)
{
    DVP_BOOL ret = DVP_FALSE;
    if (handle && pBuffer)
    {
        DVP_U32 numElem = pBuffer->numBytes/pBuffer->elemSize;
        DVP_Dim_t dims[1] = { {{{pBuffer->elemSize, numElem, 1}}} };
        ret = dvp_mem_free_import(handle, pBuffer->memType, 2, dims, 1, (DVP_PTR *)&pBuffer->pData, &hdl);
    }
    return ret;
}

DVP_KernelNode_t *DVP_KernelNode_Alloc(DVP_Handle handle, DVP_U32 numNodes)
{
    DVP_U32 i = 0;
    DVP_KernelNode_t *pNodes = NULL;
    DVP_Dim_t dims[] = {{{{sizeof(DVP_KernelNode_t), numNodes, 1}}}};
    DVP_Dim_t strs[] = {{{{0,0,0}}}};
    DVP_PTR ptrs[] = {NULL};
    if (dvp_mem_calloc(handle, DVP_MTYPE_KERNELGRAPH, dimof(ptrs), 2, dims, ptrs, strs) == DVP_TRUE)
    {
        pNodes = (DVP_KernelNode_t *)ptrs[0];
        for (i = 0; i < numNodes; i++)
        {
            pNodes[i].header.configured = DVP_FALSE;
            pNodes[i].header.affinity = DVP_CORE_MIN;
            DVP_Perf_Clear(&pNodes[i].header.perf);
        }
    }
    return pNodes;
}

void DVP_KernelNode_Free(DVP_Handle handle, DVP_KernelNode_t *pNodes, DVP_U32 numNodes)
{
    if (handle && pNodes && numNodes > 0)
    {
        DVP_Dim_t dims[] = {{{{sizeof(DVP_KernelNode_t), numNodes, 1}}}};
        DVP_PTR ptrs[] = {pNodes};
        DVP_U32 n = 0;

        // make sure the debugging handles are closed so that the data has been
        // committed to the files.
        for (n = 0; n < numNodes; n++) {
            DVP_ImageDebug_t *img = dvp_knode_to(&pNodes[n],DVP_ImageDebug_t);
#if defined(DVP_USE_FS)
            if (pNodes[n].header.kernel == DVP_KN_IMAGE_DEBUG && img->fp)
                fclose(img->fp);
            if (pNodes[n].header.kernel == DVP_KN_BUFFER_DEBUG && img->fp)
                fclose(img->fp);
#endif
        }

        memset(pNodes, 0, numNodes*sizeof(DVP_KernelNode_t));
        dvp_mem_free(handle, DVP_MTYPE_KERNELGRAPH, dimof(ptrs), 2, dims, ptrs);
    }
}
