#!/usr/bin/env python3
# Copyright (C) 2023 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License a
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from python.generators.diff_tests.testing import Path
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import DiffTestModule


class DiffTestModule_Fs(DiffTestModule):

  def test_f2fs_iostat(self):
    return DiffTestBlueprint(
        trace=Path('f2fs_iostat.textproto'),
        query=Path('f2fs_iostat_test.sql'),
        out=Path('f2fs_iostat.out'))

  def test_f2fs_iostat_latency(self):
    return DiffTestBlueprint(
        trace=Path('f2fs_iostat_latency.textproto'),
        query=Path('f2fs_iostat_latency_test.sql'),
        out=Path('f2fs_iostat_latency.out'))