#!/bin/bash

# Copyright (C) 2012 Texas Insruments, Inc.
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

dvp_test.sh clean setup
dvp_test.sh clean common simcop dsp cpu full report
dvp_test.sh clean vrun simcop full report
dvp_test.sh clean vrun2 simcop full report
dvp_test.sh clean ldc simcop full report
#dvp_test.sh clean rvm dsp cpu full report
#dvp_test.sh clean vlib dsp cpu full report
#dvp_test.sh clean imglib dsp cpu full report
cat regression_results* > regression_results_all.txt
