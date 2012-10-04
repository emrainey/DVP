#!/bin/bash

# Copyright (C) 2011 Texas Insruments, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#Open the output log
echo "**********************************************************" > regression_results.txt
echo "******************** TEST RESULTS ************************" >> regression_results.txt
echo "**********************************************************" >> regression_results.txt
echo >> regression_results.txt

if [[ "$@" =~ "full" ]]; then
    export DVP_TEST_FULL=1
fi
if [[ "$@" =~ "common" ]]; then
    export DVP_TEST_GRAPH_NUM=7
    export DVP_TEST_GRAPH_STR="common"
fi
if [[ "$@" =~ "vrun" ]]; then
    export DVP_TEST_GRAPH_NUM=8
    export DVP_TEST_GRAPH_STR="vrun"
fi
if [[ "$@" =~ "ldc" ]]; then
    export DVP_TEST_GRAPH_NUM=17
    export DVP_TEST_GRAPH_STR="ldc"
fi
if [[ "$@" =~ "imglib" ]]; then
    export DVP_TEST_GRAPH_NUM=16
    export DVP_TEST_GRAPH_STR="imglib"
fi
if [[ "$@" =~ "vlib" ]]; then
    export DVP_TEST_GRAPH_NUM=22
    export DVP_TEST_GRAPH_STR="vlib"
fi
if [[ "$@" =~ "rvm" ]]; then
    export DVP_TEST_GRAPH_NUM=1
    export DVP_TEST_GRAPH_STR="rvm"
fi



if [ -n "${MYDROID}" ]; then
    . dvp_test_android.sh $*
fi

if [ -n "${QNX_TARGET}" ]; then
    . dvp_test_qnx.sh $*
fi

if [ -z "${MYDROID}" ] && [ -z "${QNX_TARGET}" ]; then
    . dvp_test_linux.sh $*
fi
