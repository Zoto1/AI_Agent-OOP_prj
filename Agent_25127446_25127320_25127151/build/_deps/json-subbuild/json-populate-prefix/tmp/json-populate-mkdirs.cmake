# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/workspaces/AI_Agent-OOP_prj/Agent_25127446_25127320_25127151/build/_deps/json-src"
  "/workspaces/AI_Agent-OOP_prj/Agent_25127446_25127320_25127151/build/_deps/json-build"
  "/workspaces/AI_Agent-OOP_prj/Agent_25127446_25127320_25127151/build/_deps/json-subbuild/json-populate-prefix"
  "/workspaces/AI_Agent-OOP_prj/Agent_25127446_25127320_25127151/build/_deps/json-subbuild/json-populate-prefix/tmp"
  "/workspaces/AI_Agent-OOP_prj/Agent_25127446_25127320_25127151/build/_deps/json-subbuild/json-populate-prefix/src/json-populate-stamp"
  "/workspaces/AI_Agent-OOP_prj/Agent_25127446_25127320_25127151/build/_deps/json-subbuild/json-populate-prefix/src"
  "/workspaces/AI_Agent-OOP_prj/Agent_25127446_25127320_25127151/build/_deps/json-subbuild/json-populate-prefix/src/json-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/workspaces/AI_Agent-OOP_prj/Agent_25127446_25127320_25127151/build/_deps/json-subbuild/json-populate-prefix/src/json-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/workspaces/AI_Agent-OOP_prj/Agent_25127446_25127320_25127151/build/_deps/json-subbuild/json-populate-prefix/src/json-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
