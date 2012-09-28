/*
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

#ifndef _VISION_CAM_UTILS_H_
#define _VISION_CAM_UTILS_H_

#ifdef DVP_USE_ION
#   include <sosal/allocator.h>
#endif // DVP_USE_ION

#include <sosal/sosal.h>
#include <dvp/dvp_debug.h>

#define FREE_IF_PRESENT(data) { if(data) free(data); data = NULL; }

#ifdef DVP_USE_ION
#   define ALLOCATOR_HANDLES_NUM   (4)
#   define ALLOCATOR_DIMENSIONS    (1)
#endif // DVP_USE_ION

#ifndef here
#define here {printf("=======> VCam Utils - %d <=======\n", __LINE__);fflush(stdout);}
#endif // here

/** @fn void flushList(list_t *list)
   * Flushes a list data.
*/
void flushList(list_t *list);

class DataBuffer_t
{
public:
    /** @fn DataBuffer_t::DataBuffer_t(size_t allocSize )
      * Constructor.
      * Allocates a data buffers with a given size.
      * @param size_t allocSize - the size of the buffer to be allocated.
    */
    DataBuffer_t(size_t allocSize );
    ~DataBuffer_t();

    void *getData();

    size_t getSize();

    status_e push(void* data, size_t size);

private:
    /** Locks read-write operations.
    */
    mutex_t mBuffLock;

#ifdef DVP_USE_ION
    /** System Shared Mem allocator
    */
    allocator_t *pAllocator;
    _allocator_dimensions_t dims[ALLOCATOR_DIMENSIONS];
    allocator_dimensions_t strides;
    value_t handles[ALLOCATOR_HANDLES_NUM];// = { 0, 0, 0, 0 };
#endif // DVP_USE_ION

    /** Size of pDate buffer. */
    size_t nBuffSize;

    /** Used for memcpy monitoring */
    size_t bytesWritten;

    /** The buffer for 3A parameters in hardware acceptable format. */
    void *pData;

private: /// Private Methods
    /** @fn DataBuffer_t::DataBuffer_t(){}
      * Hide default constructor.
      * We want DataBuffer_t variables to be instantiated only with a valid data inside.
    */
    DataBuffer_t(){}

    /** Forbid operator=() . outside this class. */
    DataBuffer_t &operator=(DataBuffer_t &sBuff __attribute__ ((unused)) ){return *this;}
    DataBuffer_t &operator=(const DataBuffer_t &sBuff __attribute__ ((unused)) ){return *this;}
};

/** @class VisionCamExecutionService
  * This class provide the ability to register a member function and parameter size against an ID.
  * The registered function must be a class member function, and must accept a void* parameter with known size.
  * Please take care to avoid collisinos in ID values.
*/
template <class CookieType> class VisionCamExecutionService {
public:

    /** @typedef execFuncPrt_t
      * defines a type pointer to a 'Set_3A_*' member to function of 'CookieType' class.
    */
    typedef status_e (CookieType::*execFuncPrt_t)(void*);

    /** @struct execListNode_t
      * Defines a structure that holds an ID against each Set_3A_* method in this class.
    */
    typedef struct _exec_list_node_t_ {
        int32_t         optIndex;
        execFuncPrt_t   exec;
        size_t          dataSize;
        void            *data;
    }execListNode_t;

    /** @fn VisionCamExecutionService(void *cook)
      * VisionCamExecutionService constructor.
      *
      * @param cook the caller class name.
    */
    VisionCamExecutionService(void *cook) {
        mExecutionDataList = list_create();
        cookie = (CookieType*)cook;
    }

    /** @fn ~VisionCamExecutionService()
      * VisionCamExecutionService class destructor.
    */
    ~VisionCamExecutionService() {

        node_t *node = mExecutionDataList->head;
        while( node )
        {
            if( ((execListNode_t*)(node->data))->data )
            {
                free( ((execListNode_t*)(node->data))->data );
                ((execListNode_t*)(node->data))->data = NULL;
            }
            node = node->next;
        }

        flushList( mExecutionDataList );
        list_destroy(mExecutionDataList);
        mExecutionDataList = NULL;
    }

    /** @fn bool_e Register(int32_t index, execFuncPrt_t execFunc, size_t dataSize )
      * Registers a class member function against an ID and parameter size.
      *
      * @param index - integer value used as a key in function pointer list.
      * @param execFunc - pointer to a function, with return type status_e and a sigle vaoid* parameter.
      * @param dataSize - size of the parameter that this function (pointed by execFunc) accepts.
      * @return true_e on success and false_e on failure.
    */
    bool_e Register(int32_t index, execFuncPrt_t execFunc, size_t dataSize, void *data =  NULL )
    {
        bool_e ret = false_e;
        node_t *node = (node_t*)calloc(1, sizeof(node_t));
        if( node )
        {
            node->data = (value_t)calloc(1, sizeof(execListNode_t));
            if( node->data )
            {
                ((execListNode_t*)(node->data))->optIndex  = index;
                ((execListNode_t*)(node->data))->exec      = execFunc;
                ((execListNode_t*)(node->data))->dataSize  = dataSize;

                if( data && dataSize)
                {
                    ((execListNode_t*)(node->data))->data = calloc(1, dataSize);
                    memcpy( ((execListNode_t*)(node->data))->data, data, dataSize);
                }

                list_push(mExecutionDataList, node);
                ret = true_e;
            }
        }

        return ret;
    }

    /** @fn execFuncPrt_t getFunc( int32_t index )
      * retunrs a pointer to a member function that is registered against given ID.
      *
      * @param index - the ID with which the function is registered.
      * @return pointer to a function of type execFuncPrt_t.
    */
    execFuncPrt_t getFunc( int32_t index )
    {
        execListNode_t *n = getExecNode(index);
        if ( n ) return n->exec;
        return NULL;
    }

    /** @fn execFuncPrt_t getData( int32_t index )
      * retunrs a pointer to a member function that is registered against given ID.
      *
      * @param index - the ID with which the function is registered.
      * @return pointer to a data that must be passed.
    */
    void *getData( int32_t index )
    {
        execListNode_t *n = getExecNode(index);
        if( n ) return n->data;
        return NULL;
    }

    /** @fn size_t getDataSize( int32_t index )
      * Returns the size of the data that must be passed to a function registered against an ID.
      * @param index - the ID with which the function is registered.
      * @return the size of the parameter.
    */
    size_t getDataSize( int32_t index )
    {
        execListNode_t *n = getExecNode(index);
        if( n ) return n->dataSize;
        return 0;
    }

private:
    /** Instance to the caller class.
    */
    CookieType *cookie;

    /** list_t *mExecutionDataList
    */
    list_t *mExecutionDataList;

    static int nodeCompare(node_t *a, node_t *b)
    {
        int ret = -1;
        if( a && b )
        {
            execListNode_t *aa = (execListNode_t*)a->data;
            execListNode_t *bb = (execListNode_t*)b->data;

            if( aa && bb )
            {
                if( aa->optIndex == bb->optIndex ) ret = 0;
                else if( aa->optIndex < bb->optIndex ) ret = -1;
                else ret = 1;
            }
        }
        return ret;
    }

    /** @fn execListNode_t *getExecNode(int32_t index)
      * @return the data structure in which given execution function, stays beside the "index" value.
    */
    execListNode_t *getExecNode(int32_t index)
    {
        execListNode_t execNode = { index, NULL, 0, NULL };
        node_t node = { NULL, NULL, (value_t)&execNode };
        node_t *found = list_search( mExecutionDataList, &node, reinterpret_cast<node_compare_f>(nodeCompare));

        return ( found ? (execListNode_t*)(found->data) : NULL);
    }
};

#endif // _VISION_CAM_UTILS_H_
