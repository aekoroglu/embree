// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "../kernels/common/default.h"
#include "../kernels/common/ray.h"
#include "../include/embree2/rtcore.h"
#include "../include/embree2/rtcore_ray.h"
#include "rtcore_helpers.h"
#include "../tutorials/common/tutorial/application.h"

namespace embree
{
  class VerifyApplication : public Application
  {
  public:
    enum TestType { PASS, FAIL, GROUP_BEGIN, GROUP_END };
    
    struct Test : public RefCount
    {
      Test (std::string name, TestType ty = PASS) 
        : name(name), ty(ty), enabled(false) {}

      virtual bool run(VerifyApplication* state) { return false; };

    public:
      std::string name;
      TestType ty;
      bool enabled;
    };
    
  public:

    VerifyApplication ();

    void addTest(Ref<Test> test);
    void beginTestGroup(std::string name) { addTest(new Test(name,GROUP_BEGIN)); }
    void endTestGroup  () { addTest(new Test("",GROUP_END)); }
    bool runTest(Ref<Test> test, bool silent);
    void runTestGroup(size_t& id);

    int main(int argc, char** argv);
    
  public:
    RTCDevice device;
    float intensity;
    size_t numFailedTests;

  public:
    std::vector<Ref<Test>> tests;
    std::map<std::string,Ref<Test>> name2test;

  public:
    std::vector<RTCSceneFlags> sceneFlags;
    std::vector<RTCSceneFlags> sceneFlagsRobust;
    std::vector<IntersectMode> intersectModes;
    bool user_specified_tests;
    bool use_groups;
  };
}
