//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef UPDATE_ENGINE_FILESYSTEM_VERIFIER_ACTION_H_
#define UPDATE_ENGINE_FILESYSTEM_VERIFIER_ACTION_H_

#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include <chromeos/streams/stream.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "update_engine/action.h"
#include "update_engine/install_plan.h"
#include "update_engine/omaha_hash_calculator.h"

// This action will only do real work if it's a delta update. It will
// copy the root partition to install partition, and then terminate.

namespace chromeos_update_engine {

class SystemState;

// The type of filesystem that we are verifying.
enum class PartitionType {
  kSourceRootfs,
  kSourceKernel,
  kRootfs,
  kKernel,
};

// Return the partition name string for the passed partition type.
std::string PartitionTypeToString(const PartitionType partition_type);

class FilesystemVerifierAction : public InstallPlanAction {
 public:
  FilesystemVerifierAction(SystemState* system_state,
                           PartitionType partition_type);

  void PerformAction() override;
  void TerminateProcessing() override;

  // Used for testing. Return true if Cleanup() has not yet been called due
  // to a callback upon the completion or cancellation of the verifier action.
  // A test should wait until IsCleanupPending() returns false before
  // terminating the main loop.
  bool IsCleanupPending() const;

  // Debugging/logging
  static std::string StaticType() { return "FilesystemVerifierAction"; }
  std::string Type() const override { return StaticType(); }

 private:
  friend class FilesystemVerifierActionTest;
  FRIEND_TEST(FilesystemVerifierActionTest,
              RunAsRootDetermineFilesystemSizeTest);

  // Schedules the asynchronous read of the filesystem.
  void ScheduleRead();

  // Called from the main loop when a single read from |src_stream_| succeeds or
  // fails, calling OnReadDoneCallback() and OnReadErrorCallback() respectively.
  void OnReadDoneCallback(size_t bytes_read);
  void OnReadErrorCallback(const chromeos::Error* error);

  // Based on the state of the read buffer, terminates read process and the
  // action. Return whether the action was terminated.
  bool CheckTerminationConditions();

  // Cleans up all the variables we use for async operations and tells the
  // ActionProcessor we're done w/ |code| as passed in. |cancelled_| should be
  // true if TerminateProcessing() was called.
  void Cleanup(ErrorCode code);

  // Determine, if possible, the source file system size to avoid copying the
  // whole partition. Currently this supports only the root file system assuming
  // it's ext3-compatible.
  void DetermineFilesystemSize(const std::string& path);

  // The type of the partition that we are verifying.
  PartitionType partition_type_;

  // If not null, the FileStream used to read from the device.
  chromeos::StreamPtr src_stream_;

  // Buffer for storing data we read.
  chromeos::Blob buffer_;

  bool read_done_{false};  // true if reached EOF on the input stream.
  bool cancelled_{false};  // true if the action has been cancelled.

  // The install plan we're passed in via the input pipe.
  InstallPlan install_plan_;

  // Calculates the hash of the data.
  OmahaHashCalculator hasher_;

  // Reads and hashes this many bytes from the head of the input stream. This
  // field is initialized when the action is started and decremented as more
  // bytes get read.
  int64_t remaining_size_;

  // The global context for update_engine.
  SystemState* system_state_;

  DISALLOW_COPY_AND_ASSIGN(FilesystemVerifierAction);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_FILESYSTEM_VERIFIER_ACTION_H_