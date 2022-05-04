/*
 *   Copyright (c) 2021 Project CHIP Authors
 *   All rights reserved.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */
package chip.platform;

/** Interface for communicating with the CHIP mDNS stack. */
public interface ChipMdnsDiscoverCallback {
  static int START = 0;
  static int ADD = 1;
  static int REMOVE = 2;
  static int STOP = 3;

  void handleServiceDiscover(
      int event,
      String instanceName,
      String serviceType,
      String address,
      // TODO : possibly return DN & DT
      int port,
      long callbackHandle,
      long contextHandle);
}
