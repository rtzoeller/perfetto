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


class DiffTestModule_Ufs(DiffTestModule):

  def test_ufshcd_command(self):
    return DiffTestBlueprint(
        trace=Path('ufshcd_command.textproto'),
        query=Path('ufshcd_command_test.sql'),
        out=Path('ufshcd_command.out'))

  def test_ufshcd_command_tag(self):
    return DiffTestBlueprint(
        trace=Path('ufshcd_command_tag.textproto'),
        query=Path('ufshcd_command_tag_test.sql'),
        out=Path('ufshcd_command_tag.out'))
