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

#include <dvp/dvp.h>
#include <dvp/dvp_debug.h>

#if defined(DVP_USE_YUV)
#include <yuv/dvp_kl_yuv.h>
#endif

#if defined(DVP_USE_VRUN)
#include <vrun/dvp_kl_vrun.h>
#endif

uint32_t iterations;
option_t opts[] = {
    {OPTION_TYPE_INT, &iterations, sizeof(iterations), "-i", "--iter", "Iterations"},
};

//**********************************************************************
// GLOBAL VARIABLES
//**********************************************************************

DVP_CoreInfo_t cores[DVP_CORE_MAX];
DVP_U32 numCoresEnabled;

//**********************************************************************
// SUPPORT FUNCTIONS
//**********************************************************************

/*! \brief Returns first error in nodes array */
static DVP_Error_e dvp_get_error_from_nodes(DVP_KernelNode_t *nodes,
                                     DVP_U32 numNodes)
{
    DVP_U32 n = 0;
    DVP_Error_e err = DVP_SUCCESS;
    for (n = 0; n < numNodes; n++)
    {
        err = nodes[n].header.error;
        if (err != DVP_SUCCESS)
        {
            return err;
        }
    }
    return err;
}

/*! \brief Prints information when a section completes */
static void dvp_section_complete(void *cookie, DVP_KernelGraph_t *graph, DVP_U32 sectionIndex, DVP_U32 nne)
{
    // just add to the number of nodes executed.
    DVP_U32 *pCount = (DVP_U32 *)cookie;
    DVP_U32 n = 0;

    for (n = 0; n < graph->sections[sectionIndex].numNodes; n++)
    {
        DVP_PRINT(DVP_ZONE_ALWAYS, "Section[%u]:Node[%u] Kernel 0x%x Affinity:%d status=%d\n", sectionIndex, n,
            graph->sections[sectionIndex].pNodes[n].header.kernel,
            graph->sections[sectionIndex].pNodes[n].header.affinity,
            graph->sections[sectionIndex].pNodes[n].header.error);
    }
    *pCount += nne;
}

//**********************************************************************
// UNITTESTS
//**********************************************************************

/*! \brief Tests the creation and destruction of a DVP Handle.
 * \return Returns status_e
 * \retval STATUS_SUCCESS on success.
 * \retval STATUS_FAILURE on failure.
 * \ingroup group_tests
 */
status_e dvp_context_test(void)
{
    status_e status = STATUS_FAILURE;
    DVP_Handle dvp = DVP_KernelGraph_Init();
    if (dvp)
    {
        status = STATUS_SUCCESS;
        DVP_KernelGraph_Deinit(dvp);
    }
    return status;
}

/*! \brief Tests the \ref DVP_QuerySystem API.
 * \return Returns status_e
 * \retval STATUS_SUCCESS on success.
 * \retval STATUS_FAILURE on failure.
 * \ingroup group_tests
 */
status_e dvp_info_test(void)
{
    status_e status = STATUS_FAILURE;
    DVP_Handle dvp = DVP_KernelGraph_Init();
    if (dvp)
    {
        DVP_U32 c = 0;
        memset(cores, 0, sizeof(cores));
        numCoresEnabled = 0;

        DVP_QuerySystem(dvp, cores);

        for (c = DVP_CORE_MIN+1; c < DVP_CORE_MAX; c++)
        {
            if (cores[c].enabled)
            {
                status = STATUS_SUCCESS;
                numCoresEnabled++;
            }
        }
        DVP_PRINT(DVP_ZONE_ALWAYS, "%u cores enabled!\n",  numCoresEnabled);
        DVP_KernelGraph_Deinit(dvp);
    }
    return status;
}

/*! \brief Tests a single node NO-OP graph on CPU.
 * \return Returns status_e
 * \retval STATUS_SUCCESS on success.
 * \retval STATUS_FAILURE on failure.
 * \ingroup group_tests
 */
status_e dvp_cpu_nop_test(void)
{
    status_e status = STATUS_FAILURE;
    DVP_Handle dvp = DVP_KernelGraph_Init();
    if (dvp)
    {
        DVP_U32 numNodes = 1;
        DVP_U32 numSections = 1;
        DVP_U32 numNodesExecuted = 0;
        DVP_KernelNode_t *nodes = DVP_KernelNode_Alloc(dvp, numNodes);
        if (nodes)
        {
            DVP_KernelGraph_t *graph = DVP_KernelGraph_Alloc(dvp, numSections);
            if (graph)
            {
                DVP_Error_e err = DVP_SUCCESS;
                err = DVP_KernelGraphSection_Init(dvp, graph, 0, nodes, numNodes);
                if (err == DVP_SUCCESS)
                {
                    DVP_U32 numSectionsRun = 0;

                    nodes[0].header.kernel = DVP_KN_NOOP;
                    nodes[0].header.affinity = DVP_CORE_CPU;

                    numSectionsRun = DVP_KernelGraph_Process(dvp, graph, &numNodesExecuted, dvp_section_complete);
                    err = dvp_get_error_from_nodes(nodes, numNodes);
                    if (numSectionsRun == numSections && numNodesExecuted == numNodes && err == DVP_SUCCESS)
                    {
                        status = STATUS_SUCCESS;
                    }
                    DVP_PRINT(DVP_ZONE_ALWAYS, "CPU NOP processed %u sections, %u nodes, first DVP_Error_e=%d\n", numSectionsRun, numNodesExecuted, err);
                }
                DVP_KernelGraph_Free(dvp, graph);
                graph = NULL;
            }
            DVP_KernelNode_Free(dvp, nodes, numNodes);
            nodes = NULL;
        }
        DVP_KernelGraph_Deinit(dvp);
    }
    return status;
}


/*! \brief Tests a serial array of nodes graph on all available cores.
 * \return Returns status_e
 * \retval STATUS_SUCCESS on success.
 * \retval STATUS_FAILURE on failure.
 * \ingroup group_tests
 */
status_e dvp_serial_nop_test(void)
{
    status_e status = STATUS_FAILURE;
    DVP_Handle dvp = DVP_KernelGraph_Init();
    if (dvp)
    {
        DVP_U32 numSections = 1;
        DVP_U32 numNodes = numCoresEnabled;
        DVP_U32 numNodesExecuted = 0;
        DVP_KernelNode_t *nodes = DVP_KernelNode_Alloc(dvp, numNodes);
        if (nodes)
        {
            DVP_KernelGraph_t *graph = DVP_KernelGraph_Alloc(dvp, numSections);
            if (graph)
            {
                DVP_Error_e err = DVP_SUCCESS;
                err = DVP_KernelGraphSection_Init(dvp, graph, 0, nodes, numNodes);
                if (err == DVP_SUCCESS)
                {
                    DVP_U32 n, numSectionsRun = 0;
                    DVP_Core_e c;

                    for (n = 0, c = DVP_CORE_MIN+1; c < DVP_CORE_MAX; c++)
                    {
                        if (cores[c].enabled == DVP_TRUE)
                        {
                            nodes[n].header.kernel = DVP_KN_NOOP;
                            nodes[n].header.affinity = (DVP_Core_e)c;
                            n++;
                        }
                    }

                    numSectionsRun = DVP_KernelGraph_Process(dvp, graph, &numNodesExecuted, dvp_section_complete);
                    err = dvp_get_error_from_nodes(nodes, numNodes);
                    if (numSectionsRun == numSections && numNodesExecuted == numNodes && err == DVP_SUCCESS)
                    {
                        status = STATUS_SUCCESS;
                    }
                    DVP_PRINT(DVP_ZONE_ALWAYS, "SERIAL NOP processed %u sections, %u nodes, first DVP_Error_e=%d\n", numSectionsRun, numNodesExecuted, err);
                }
                DVP_KernelGraph_Free(dvp, graph);
                graph = NULL;
            }
            DVP_KernelNode_Free(dvp, nodes, numNodes);
            nodes = NULL;
        }
        DVP_KernelGraph_Deinit(dvp);
        dvp = 0;
    }
    return status;
}

/*! \brief Tests a "parallel set of nodes" graph on all available cores.
 * \return Returns status_e
 * \retval STATUS_SUCCESS on success.
 * \retval STATUS_FAILURE on failure.
 * \ingroup group_tests
 */
status_e dvp_parallel_nop_test(void)
{
    status_e status = STATUS_FAILURE;
    DVP_Handle dvp = DVP_KernelGraph_Init();
    if (dvp)
    {
        DVP_U32 numNodes = numCoresEnabled;
        DVP_U32 numSections = numCoresEnabled;
        DVP_U32 numNodesExecuted = 0;
        DVP_KernelNode_t *nodes = DVP_KernelNode_Alloc(dvp, numNodes);
        if (nodes)
        {
            DVP_KernelGraph_t *graph = DVP_KernelGraph_Alloc(dvp, numSections);
            if (graph)
            {
                DVP_Error_e err = DVP_SUCCESS;
                DVP_U32 n, s = 0;
                DVP_Core_e c;

                // one node per section...
                for (s = 0; s < numSections; s++)
                {
                    err = DVP_KernelGraphSection_Init(dvp, graph, s, &nodes[s], 1);
                    if (err != DVP_SUCCESS)
                    {
                        DVP_KernelGraph_Free(dvp, graph);
                        DVP_KernelNode_Free(dvp, nodes, numNodes);
                        return STATUS_NOT_ENOUGH_MEMORY;
                    }
                }

                if (err == DVP_SUCCESS)
                {
                    DVP_U32 numSectionsRun = 0;

                    // set the core affinity to all the available cores.
                    for (n = 0, c = DVP_CORE_MIN+1; c < DVP_CORE_MAX; c++)
                    {
                        if (cores[c].enabled == DVP_TRUE)
                        {
                            nodes[n].header.kernel = DVP_KN_NOOP;
                            nodes[n].header.affinity = (DVP_Core_e)c;
                            n++;
                        }
                    }

                    numSectionsRun = DVP_KernelGraph_Process(dvp, graph, &numNodesExecuted, dvp_section_complete);
                    err = dvp_get_error_from_nodes(nodes, numNodes);
                    if (numSectionsRun == numSections && numNodesExecuted == numNodes && err == DVP_SUCCESS)
                    {
                        status = STATUS_SUCCESS;
                    }
                    DVP_PRINT(DVP_ZONE_ALWAYS, "PARALLEL NOP processed %u sections, %u nodes, first DVP_Error_e=%d\n", numSectionsRun, numNodesExecuted, err);
                }
                DVP_KernelGraph_Free(dvp, graph);
                graph = NULL;
            }
            DVP_KernelNode_Free(dvp, nodes, numNodes);
            nodes = NULL;
        }
        DVP_KernelGraph_Deinit(dvp);
    }
    return status;
}

/*! \brief Tests a set of copy nodes in series on all available cores.
 * \return Returns status_e
 * \retval STATUS_SUCCESS on success.
 * \retval STATUS_FAILURE on failure.
 * \ingroup group_tests
 */
status_e dvp_copy_test(void)
{
    status_e status = STATUS_FAILURE;
    DVP_MemType_e mtype = DVP_MTYPE_DEFAULT;
    DVP_Handle dvp = DVP_KernelGraph_Init();
#if defined(DVP_USE_BO) || defined(DVP_USE_ION)
    mtype = DVP_MTYPE_MPUCACHED_1DTILED;
#endif
    if (dvp)
    {
        DVP_U32 numSections = 1;
        DVP_U32 numNodes = numCoresEnabled;
        DVP_U32 numNodesExecuted = 0;
        DVP_KernelNode_t *nodes = DVP_KernelNode_Alloc(dvp, numNodes);
        if (nodes)
        {
            DVP_KernelGraph_t *graph = DVP_KernelGraph_Alloc(dvp, numSections);
            if (graph)
            {
                DVP_Error_e err = DVP_SUCCESS;
                err = DVP_KernelGraphSection_Init(dvp, graph, 0, nodes, numNodes);
                if (err == DVP_SUCCESS)
                {
#define width 320
#define height 240
                    DVP_U32 n, numSectionsRun = 0;
                    DVP_Core_e c;
                    DVP_U08 ptr[height][width];
                    memset(ptr, 0x57, sizeof(ptr));

                    for (n = 0, c = 0; c < DVP_CORE_MAX; c++)
                    {
                        if (cores[c].enabled == DVP_TRUE)
                        {
                            DVP_Transform_t *pT = dvp_knode_to(&nodes[n], DVP_Transform_t);
                            nodes[n].header.kernel = DVP_KN_COPY;
                            nodes[n].header.affinity = (DVP_Core_e)c;

                            DVP_Image_Init(&pT->input, width, height, FOURCC_Y800);
                            DVP_Image_Alloc(dvp, &pT->input, mtype);

                            DVP_Image_Fill(&pT->input, (DVP_S08 *)ptr, width*height*sizeof(DVP_U08));

                            DVP_Image_Init(&pT->output, width, height, FOURCC_Y800);
                            DVP_Image_Alloc(dvp, &pT->output, mtype);

                            n++;
                        }
                    }

                    numSectionsRun = DVP_KernelGraph_Process(dvp, graph, &numNodesExecuted, dvp_section_complete);
                    err = dvp_get_error_from_nodes(nodes, numNodes);

                    for (n = 0; n < numNodes; n++)
                    {
                        DVP_Transform_t *pT = dvp_knode_to(&nodes[n], DVP_Transform_t);
                        if (DVP_Image_Equal(&pT->output, &pT->input) == DVP_FALSE)
                        {
                            DVP_PRINT(DVP_ZONE_ERROR, "Output != Input on Node %d, Affinity %d\n", n, nodes[n].header.affinity);
                            err = DVP_ERROR_FAILURE;
                        }
                        DVP_Image_Free(dvp, &pT->input);
                        DVP_Image_Free(dvp, &pT->output);
                    }

                    if (numSectionsRun == numSections && numNodesExecuted == numNodes && err == DVP_SUCCESS)
                    {
                        status = STATUS_SUCCESS;
                    }
                    DVP_PRINT(DVP_ZONE_ALWAYS, "Serial Copy processed %u sections, %u nodes, first DVP_Error_e=%d\n", numSectionsRun, numNodesExecuted, err);
                }
                DVP_KernelGraph_Free(dvp, graph);
                graph = NULL;
            }
            DVP_KernelNode_Free(dvp, nodes, numNodes);
            nodes = NULL;
        }
        DVP_KernelGraph_Deinit(dvp);
        dvp = 0;
    }
    return status;
#undef width
#undef height
}

/*! \brief Tests a serial/parallel/serial copy graph on all available cores.
 * \return Returns status_e
 * \retval STATUS_SUCCESS on success.
 * \retval STATUS_FAILURE on failure.
 * \ingroup group_tests
 */
status_e dvp_custom_copy_test(void)
{
    status_e status = STATUS_FAILURE;

    DVP_U32 numSections = 2 + numCoresEnabled;
    DVP_U32 numNodes = 3 * numCoresEnabled;

    DVP_U32 section_index = 0, image_index = 0, in_index;
    DVP_KernelNode_t *section_node;

    DVP_U32 numNodesExecuted = 0;
    DVP_U32 numSectionsRun = 0;

    DVP_Core_e c;
    DVP_Transform_t *pT;

    DVP_U32 width = 640, height = 480;
    DVP_U32 numImages = numNodes + 1;
    DVP_Image_t images[numImages];

    DVP_U32 n;
    DVP_Error_e err = DVP_SUCCESS;

    DVP_U08 ptr[height][width];
    memset(ptr, 0x57, sizeof(ptr));


    DVP_Handle dvp = DVP_KernelGraph_Init();
    if (dvp == 0)
        return status;

    DVP_KernelNode_t *nodes = DVP_KernelNode_Alloc(dvp, numNodes);
    if (nodes == NULL)
    {
        DVP_KernelGraph_Deinit(dvp);
        return STATUS_NOT_ENOUGH_MEMORY;
    }

    DVP_KernelGraph_t *graph = DVP_KernelGraph_Alloc(dvp, numSections);
    if (graph == NULL)
    {
        DVP_KernelNode_Free(dvp, nodes, numNodes);
        DVP_KernelGraph_Deinit(dvp);
        return STATUS_NOT_ENOUGH_MEMORY;
    }

    for (n = 0; n < numImages; n++)
    {
        DVP_Image_Init(&images[n], width, height, FOURCC_Y800);
        DVP_Image_Alloc(dvp, &images[n], DVP_MTYPE_DEFAULT);
    }

    section_node = nodes;

    err = DVP_KernelGraphSection_Init(dvp, graph, section_index, section_node, numCoresEnabled);
    if (err != DVP_SUCCESS)
    {
        goto exit;
    }
    graph->order[section_index] = 0;

    for (n = 0; n < numCoresEnabled; n++)
    {
        pT = dvp_knode_to(&section_node[n], DVP_Transform_t);
        pT->input = images[image_index];
        pT->output = images[image_index + 1];

        image_index ++;
    }

    section_index ++;
    section_node += numCoresEnabled;

    in_index = image_index;

    // one node per section...
    for (n = 0; n < numCoresEnabled; n++)
    {
        image_index ++;

        err = DVP_KernelGraphSection_Init(dvp, graph, section_index, section_node, 1);
        if (err != DVP_SUCCESS)
        {
            status = STATUS_NOT_ENOUGH_MEMORY;
            goto exit;
        }
        graph->order[section_index] = 1;

        pT = dvp_knode_to(section_node, DVP_Transform_t);
        pT->input = images[in_index];
        pT->output = images[image_index];

        section_node ++;
        section_index ++;
    }

    err = DVP_KernelGraphSection_Init(dvp, graph, section_index, section_node, numCoresEnabled);
    if (err != DVP_SUCCESS)
    {
        goto exit;
    }
    graph->order[section_index] = 2;

    for (n = 0; n < numCoresEnabled; n++)
    {
        pT = dvp_knode_to(&section_node[n], DVP_Transform_t);
        pT->input = images[image_index];
        pT->output = images[image_index + 1];

        image_index ++;
    }

    // set to last image
    section_node += numCoresEnabled - 1;

    for (n = 0, c = DVP_CORE_MIN+1; c < DVP_CORE_MAX; c++)
    {
        if (cores[c].enabled == DVP_TRUE)
        {
            nodes[n].header.kernel = DVP_KN_COPY;
            nodes[n].header.affinity = (DVP_Core_e)c;

            nodes[n + numCoresEnabled].header.kernel = DVP_KN_COPY;
            nodes[n + numCoresEnabled].header.affinity = (DVP_Core_e)c;

            nodes[n + 2 * numCoresEnabled].header.kernel = DVP_KN_COPY;
            nodes[n + 2 * numCoresEnabled].header.affinity = (DVP_Core_e)c;

            n++;
        }
    }

    pT = dvp_knode_to(&nodes[0], DVP_Transform_t);
    DVP_Image_Fill(&pT->input, (DVP_S08 *)ptr, width*height*sizeof(DVP_U08));

    numSectionsRun = DVP_KernelGraph_Process(dvp, graph, &numNodesExecuted, dvp_section_complete);

    err = dvp_get_error_from_nodes(nodes, numNodes);
    if (numSectionsRun == numSections && numNodesExecuted == numNodes && err == DVP_SUCCESS)
    {
        DVP_Transform_t *pT1 = dvp_knode_to(section_node, DVP_Transform_t);
        if (DVP_Image_Equal(&pT->input, &pT1->output) == DVP_FALSE)
        {
            DVP_PRINT(DVP_ZONE_ERROR, "dvp_custom_copy_test: Output != Input\n");
        }
        else
            status = STATUS_SUCCESS;
    }

    DVP_PRINT(DVP_ZONE_ALWAYS, "CUSTOM COPY processed %u sections, %u nodes, first DVP_Error_e=%d\n", numSectionsRun, numNodesExecuted, err);

exit:
    for (n = 0; n < numImages; n++)
    {
        DVP_Image_Free(dvp, &images[n]);
        DVP_Image_Deinit(&images[n]);
    }

    DVP_KernelGraph_Free(dvp, graph);
    DVP_KernelNode_Free(dvp, nodes, numNodes);
    DVP_KernelGraph_Deinit(dvp);

    return status;
}


#if defined(DVP_USE_YUV)
/*! \brief Tests a split image color conversion.
 * \return Returns status_e
 * \retval STATUS_SUCCESS on success.
 * \retval STATUS_FAILURE on failure.
 * \ingroup group_tests
 */
status_e dvp_split_cc_test(void)
{
    status_e status = STATUS_FAILURE;
    DVP_Handle dvp = DVP_KernelGraph_Init();
    if (dvp)
    {
        DVP_U32 numNodes = 2;
        DVP_U32 numSections = 2;
        DVP_U32 numNodesExecuted = 0;
        DVP_KernelNode_t *nodes = DVP_KernelNode_Alloc(dvp, numNodes);
        if (nodes)
        {
            DVP_KernelGraph_t *graph = DVP_KernelGraph_Alloc(dvp, numSections);
            if (graph)
            {
                DVP_Error_e err = DVP_SUCCESS;
                err = DVP_KernelGraphSection_Init(dvp, graph, 0, nodes, numNodes);
                if (err == DVP_SUCCESS)
                {
                    DVP_Image_t images[2];
                    DVP_U32 numSectionsRun = 0;
                    DVP_Transform_t *io[2];
                    DVP_Image_t *img[4];

                    DVP_Image_Init(&images[0], 1280, 720, FOURCC_ARGB);
                    DVP_Image_Init(&images[1], 1280, 720, FOURCC_UYVY);

                    DVP_Image_Alloc(dvp, &images[0], DVP_MTYPE_MPUCACHED_VIRTUAL);
                    DVP_Image_Alloc(dvp, &images[1], DVP_MTYPE_MPUCACHED_VIRTUAL);

                    nodes[0].header.kernel = DVP_KN_YUV_ARGB_TO_UYVY;
                    nodes[0].header.affinity = DVP_CORE_CPU;
                    io[0] = dvp_knode_to(&nodes[0],DVP_Transform_t);
                    DVP_Image_Dup(&io[0]->input, &images[0]);
                    DVP_Image_Dup(&io[0]->output, &images[1]);

                    nodes[1].header.kernel = DVP_KN_YUV_ARGB_TO_UYVY;
                    nodes[1].header.affinity = DVP_CORE_CPU;
                    io[1] = dvp_knode_to(&nodes[1],DVP_Transform_t);
                    DVP_Image_Dup(&io[1]->input, &images[0]);
                    DVP_Image_Dup(&io[1]->output, &images[1]);

                    // now perform some arimethic on the pointers to make each
                    // node cover half the image.
                    img[0] = &io[0]->input;
                    img[1] = &io[0]->output;
                    img[2] = &io[1]->input;
                    img[3] = &io[1]->output;

                    img[0]->height /= 2;
                    img[1]->height /= 2;
                    img[2]->height /= 2;
                    img[3]->height /= 2;

                    // move the pointer 1/2 down the image
                    img[2]->pData[0] = &img[2]->pData[0][img[2]->height * img[2]->y_stride];
                    img[3]->pData[0] = &img[3]->pData[0][img[3]->height * img[3]->y_stride];

                    // assign each node to a separate section
                    DVP_KernelGraphSection_Init(dvp, graph, 0, &nodes[0], 1);
                    DVP_KernelGraphSection_Init(dvp, graph, 1, &nodes[1], 1);

                    numSectionsRun = DVP_KernelGraph_Process(dvp, graph, &numNodesExecuted, dvp_section_complete);
                    err = dvp_get_error_from_nodes(nodes, numNodes);
                    if (numSectionsRun == numSections && numNodesExecuted == numNodes && err == DVP_SUCCESS)
                    {
                        status = STATUS_SUCCESS;
                    }

                    DVP_Image_Free(dvp, &images[0]);
                    DVP_Image_Free(dvp, &images[1]);

                    DVP_PRINT(DVP_ZONE_ALWAYS, "CPU CC processed %u sections, %u nodes, first DVP_Error_e=%d\n", numSectionsRun, numNodesExecuted, err);
                    DVP_PrintPerformanceGraph(dvp, graph);
                }
                DVP_KernelGraph_Free(dvp, graph);
                graph = NULL;
            }
            DVP_KernelNode_Free(dvp, nodes, numNodes);
            nodes = NULL;
        }
        DVP_KernelGraph_Deinit(dvp);
    }
    return status;
}
#endif

status_e dvp_imageshift_test(void)
{
    status_e status = STATUS_FAILURE;
    DVP_Handle dvp = DVP_KernelGraph_Init();
    if (dvp)
    {
        DVP_U32 numNodes = 1;
        DVP_U32 numSections = 1;
        dvp_image_shift_t shift;
        DVP_KernelNode_t *nodes = DVP_KernelNode_Alloc(dvp, numNodes);
        if (nodes)
        {
            DVP_KernelGraph_t *graph = DVP_KernelGraph_Alloc(dvp, numSections);
            if (graph)
            {
                DVP_Error_e err = DVP_SUCCESS;
                err = DVP_KernelGraphSection_Init(dvp, graph, 0, nodes, numNodes);
                if (err == DVP_SUCCESS)
                {
                    DVP_Image_t images[3];
                    DVP_ImageConvolution_t *pT = dvp_knode_to(&nodes[0], DVP_ImageConvolution_t);
                    DVP_S08 mask[] = {1,2,1,
                                      2,4,2,
                                      1,2,1};

                    DVP_Image_Init(&images[0], 640, 480, FOURCC_Y800);
                    DVP_Image_Init(&images[1], 640, 480, FOURCC_Y800);
                    DVP_Image_Init(&images[2], 3, 3, FOURCC_Y800);

                    DVP_Image_Alloc(dvp, &images[0], DVP_MTYPE_DEFAULT);
                    DVP_Image_Alloc(dvp, &images[1], DVP_MTYPE_DEFAULT);
                    DVP_Image_Alloc(dvp, &images[2], DVP_MTYPE_DEFAULT);

                    DVP_Image_Fill(&images[2], mask, sizeof(mask));

#if defined(DVP_USE_VRUN)
                    nodes[0].header.kernel = DVP_KN_VRUN_CONV_MxN;
                    nodes[0].header.affinity = DVP_CORE_SIMCOP;
#endif
                    DVP_Image_Dup(&pT->input, &images[0]);
                    DVP_Image_Dup(&pT->output, &images[1]);
                    DVP_Image_Dup(&pT->mask, &images[2]);
                    pT->shiftMask = 4;

                    if (DVP_KernelGraph_Verify(dvp, graph))
                    {
                        memset(&shift, 0, sizeof(shift));
                        DVP_KernelGraph_ImageShiftAccum(dvp, &nodes[0], &shift);

                        DVP_PRINT(DVP_ZONE_ALWAYS, "ImageShift = {%d, %d}\n", shift.centerShiftHorz, shift.centerShiftVert);

                        if (shift.centerShiftHorz == -(DVP_S32)images[2].width/2 &&
                            shift.centerShiftVert == -(DVP_S32)images[2].height/2)
                            status = STATUS_SUCCESS;
                    }
                }
                DVP_KernelGraph_Free(dvp, graph);
                graph = NULL;
            }
            DVP_KernelNode_Free(dvp, nodes, numNodes);
            nodes = NULL;
        }
        DVP_KernelGraph_Deinit(dvp);
    }
    return status;
}

/*! \brief Local Unit Test Function Pointer */
typedef status_e (*dvp_unittest_f)(void);

/*! \brief Unit test meta-data structure used to track result of each function */
typedef struct _dvp_unittest_t {
    status_e status;
    char name[MAX_PATH];
    dvp_unittest_f function;
} dvp_unittest_t;

dvp_unittest_t unittests[] = {
    {STATUS_FAILURE, "Framework: Context", dvp_context_test},
    {STATUS_FAILURE, "Framework: QuerySystem", dvp_info_test},
    {STATUS_FAILURE, "Framework: CPU Nop Test", dvp_cpu_nop_test},
    {STATUS_FAILURE, "Framework: SERIAL Nop Test", dvp_serial_nop_test},
    {STATUS_FAILURE, "Framework: PARALLEL Nop Test", dvp_parallel_nop_test},
    {STATUS_FAILURE, "Framework: SERIAL Copy Test", dvp_copy_test},
    {STATUS_FAILURE, "Framework: CUSTOM Copy Test", dvp_custom_copy_test},
#if defined(DVP_USE_YUV)
    {STATUS_FAILURE, "Framework: PARALLEL CC Test", dvp_split_cc_test},
#endif
    {STATUS_FAILURE, "Framework: ImageShift", dvp_imageshift_test},
};

int main(int argc, char *argv[])
{
    uint32_t i,j;
    int err = 0;
    int passed = 0;
    iterations = 1;

#if defined(SOSAL_RUNTIME_DEBUG)
    debug_get_zone_mask("SOSAL_ZONE_MASK", &sosal_zone_mask);
#endif

#if defined(DVP_RUNTIME_DEBUG)
    debug_get_zone_mask("DVP_ZONE_MASK", &dvp_zone_mask);
#endif

    option_process(argc, argv, opts, dimof(opts));
    for (i = 0; i < dimof(unittests); i++)
    {
        for (j = 0; j < iterations; j++)
        {
            unittests[i].status = unittests[i].function();
            if (unittests[i].status == STATUS_SUCCESS)
                passed++;
        }
        DVP_PRINT(DVP_ZONE_ALWAYS, "[%s] %s\n", (unittests[i].status == STATUS_SUCCESS?"PASSED":"FAILED"), unittests[i].name);
    }
    DVP_PRINT(DVP_ZONE_ALWAYS, "Passed %d out of %u\n", passed, dimof(unittests));
    return err;
}

