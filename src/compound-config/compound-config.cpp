/* Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <fstream>
#include <cstring>
#include <streambuf>

#include "compound-config/compound-config.hpp"
#include "compound-config/hyphens-to-underscores.hpp"

#define EXCEPTION_PROLOGUE                                                          \
    try { 

#define EXCEPTION_EPILOGUE                                                          \
    }                                                                               \
    catch (const libconfig::SettingTypeException& e)                                \
    {                                                                               \
      std::cerr << "ERROR: setting type exception at: " << e.getPath() << std::endl;\
      exit(1);                                                                      \
    }                                                                               \
    catch (const libconfig::SettingNotFoundException& e)                            \
    {                                                                               \
      std::cerr << "ERROR: setting not found: " << e.getPath() << std::endl;        \
      exit(1);                                                                      \
    }                                                                               \
    catch (const libconfig::SettingNameException& e)                                \
    {                                                                               \
      std::cerr << "ERROR: setting name exception at: " << e.getPath() << std::endl;\
      exit(1);                                                                      \
    }                                                                               \
    catch (YAML::KeyNotFound& e)                                                     \
    {                                                                               \
      std::cerr << "ERROR: " << e.msg << ", at line: " << e.mark.line+1<< std::endl;\
      exit(1);                                                                      \
    }                                                                               \
    catch (YAML::InvalidNode& e)                                                     \
    {                                                                               \
      std::cerr << "ERROR: " << e.msg << ", at line: " << e.mark.line+1<< std::endl;\
      exit(1);                                                                      \
    }                                                                               \
    catch (YAML::BadConversion& e)                                                   \
    {                                                                               \
      std::cerr << "ERROR: " << e.msg << ", at line: " << e.mark.line+1<< std::endl;\
      exit(1);                                                                      \
    }                                                                               \


namespace config
{

/* CompoundConfigNode */

CompoundConfigNode::CompoundConfigNode(libconfig::Setting* _lnode, YAML::Node _ynode) {
  LNode = _lnode;
  YNode = _ynode;
}

CompoundConfigNode::CompoundConfigNode(libconfig::Setting* _lnode, YAML::Node _ynode, CompoundConfig* _cConfig) {
  LNode = _lnode;
  YNode = _ynode;
  cConfig = _cConfig;
}

CompoundConfigNode CompoundConfigNode::lookup(const char *path) const {
  EXCEPTION_PROLOGUE;
  if (LNode) {
    libconfig::Setting& nextNode = LNode->lookup(path);
    return CompoundConfigNode(&nextNode, YAML::Node(), cConfig);
  } else if (YNode) {
    if (!YNode[path]) {
      // The best implementation is to just throw exception here, but
      // yaml-cpp-0.5 API does not have Node::Mark(), so this is a workaround
      // to force an exception and get the Mark and then throw the correct
      // exception on our own.
      try {
        YNode[path].as<int>(); // force an exception!
      } catch (YAML::Exception& e) {
        throw YAML::KeyNotFound(e.mark, std::string(path));
      }
    }
    YAML::Node nextNode = YNode[path];
    return CompoundConfigNode(nullptr, nextNode, cConfig);
  } else {
    assert(false);
  }
  EXCEPTION_EPILOGUE;
}


bool CompoundConfigNode::lookup(const char *path, CompoundConfigNode &result) const {
  if (LNode) {
    if (!LNode->exists(path)) {
      return false; // Return false if the path is not found
    }
    libconfig::Setting& nextNode = LNode->lookup(path);
    result = CompoundConfigNode(&nextNode, YAML::Node(), cConfig);
    return true;
  } else if (YNode) {
    if (!YNode[path]) {
      return false; // Return false if the path is not found
    }
    YAML::Node nextNode = YNode[path];
    result = CompoundConfigNode(nullptr, nextNode, cConfig);
    return true;
  } else {
    assert(false); // This shouldn't happen
    return false;
  }
}


bool CompoundConfigNode::lookupValue(const char *name, bool &value) const {
  EXCEPTION_PROLOGUE;
  if (LNode) return LNode->lookupValue(name, value);
  else if (YNode) {
    if (YNode.IsScalar() || !YNode[name].IsDefined() || !YNode[name].IsScalar()) return false;
    try {
      value = YNode[name].as<bool>();
    } catch (YAML::BadConversion& e) {
      std::string variableName = YNode[name].as<std::string>();
      if (cConfig->getVariableRoot().exists(variableName)) {
        cConfig->getVariableRoot().lookupValue(variableName, value);
      } else {
        std::cerr << "Cannot find " << variableName << " for " << name << " under root key: variables" << std::endl;
        throw e;
      }
    }
    return true;
  }
  else {
    assert(false);
    return false;
  }
  EXCEPTION_EPILOGUE;
}

bool CompoundConfigNode::lookupValue(const char *name, int &value) const {
  EXCEPTION_PROLOGUE;
  if (LNode) {
    if (!LNode->lookupValue(name, value)) {
      std::string variableName;
      if (LNode->lookupValue(name, variableName) &&
          cConfig->getVariableRoot().exists(variableName) ) {
        return cConfig->getVariableRoot().lookupValue(variableName, value);
      } else return false;
    } else return true;
  }
  else if (YNode) {
    if (YNode.IsScalar() || !YNode[name].IsDefined() || !YNode[name].IsScalar()) return false;
    try {
      value = YNode[name].as<int>();
    } catch (YAML::BadConversion& e) {
      std::string variableName = YNode[name].as<std::string>();
      if (cConfig->getVariableRoot().exists(variableName)) {
        cConfig->getVariableRoot().lookupValue(variableName, value);
      } else {
        std::cerr << "Cannot find " << variableName << " for " << name << " under root key: variables" << std::endl;
        throw e;
      }
    }
    return true;
  }
  else {
    assert(false);
    return false;
  }
  EXCEPTION_EPILOGUE;
}

bool CompoundConfigNode::lookupValue(const char *name, unsigned int &value) const {
  EXCEPTION_PROLOGUE;
  if (LNode) {
    if (!LNode->lookupValue(name, value)) {
      std::string variableName;
      if (LNode->lookupValue(name, variableName) &&
          cConfig->getVariableRoot().exists(variableName) ) {
        return cConfig->getVariableRoot().lookupValue(variableName, value);
      } else return false;
    } else return true;
  }
  else if (YNode) {
    if (YNode.IsScalar() || !YNode[name].IsDefined() || !YNode[name].IsScalar()) return false;
    try {
      value = YNode[name].as<unsigned int>();
    } catch (YAML::BadConversion& e) {
      std::string variableName = YNode[name].as<std::string>();
      if (cConfig->getVariableRoot().exists(variableName)) {
        cConfig->getVariableRoot().lookupValue(variableName, value);
      } else {
        std::cerr << "Cannot find " << variableName << " for " << name << " under root key: variables" << std::endl;
        throw e;
      }
    }
    return true;
  }
  else {
    assert(false);
    return false;
  }
  EXCEPTION_EPILOGUE;

}

bool CompoundConfigNode::lookupValueLongOnly(const char *name, long long &value) const {
  EXCEPTION_PROLOGUE;
  if (LNode) {
    if (!LNode->lookupValue(name, value)) {
      std::string variableName;
      if (LNode->lookupValue(name, variableName) &&
          cConfig->getVariableRoot().exists(variableName) ) {
        return cConfig->getVariableRoot().lookupValue(variableName, value);
      } else return false;
    } else return true;
  }
  else if (YNode) {
    if (YNode.IsScalar() || !YNode[name].IsDefined() || !YNode[name].IsScalar()) return false;
    try {
      value = YNode[name].as<long long>();
    } catch (YAML::BadConversion& e) {
      std::string variableName = YNode[name].as<std::string>();
      if (cConfig->getVariableRoot().exists(variableName)) {
        cConfig->getVariableRoot().lookupValue(variableName, value);
      } else {
        std::cerr << "Cannot find " << variableName << " for " << name << " under root key: variables" << std::endl;
        throw e;
      }
    }
    return true;
  }
  else {
    assert(false);
    return false;
  }
  EXCEPTION_EPILOGUE;
}

bool CompoundConfigNode::lookupValue(const char *name, long long &value) const {
  if(lookupValueLongOnly(name, value)) return true; // Reads values of the form 123L
  int int_value;
  if(lookupValue(name, int_value)) { // Reads normal integers
    value = (long long) int_value;
    return true;
  }
  return false;
}

bool CompoundConfigNode::lookupValueLongOnly(const char *name, unsigned long long &value) const {
  EXCEPTION_PROLOGUE;
  if (LNode) {
    if (!LNode->lookupValue(name, value)) {
      std::string variableName;
      if (LNode->lookupValue(name, variableName) &&
          cConfig->getVariableRoot().exists(variableName) ) {
        return cConfig->getVariableRoot().lookupValue(variableName, value);
      } else return false;
    } else return true;
  }
  else if (YNode) {
    if (YNode.IsScalar() || !YNode[name].IsDefined() || !YNode[name].IsScalar()) return false;
    try {
      value = YNode[name].as<unsigned long long>();
    } catch (YAML::BadConversion& e) {
      std::string variableName = YNode[name].as<std::string>();
      if (cConfig->getVariableRoot().exists(variableName)) {
        cConfig->getVariableRoot().lookupValue(variableName, value);
      } else {
        std::cerr << "Cannot find " << variableName << " for " << name << " under root key: variables" << std::endl;
        throw e;
      }
    }
    return true;
  }
  else {
    assert(false);
    return false;
  }
  EXCEPTION_EPILOGUE;
}

bool CompoundConfigNode::lookupValue(const char *name, unsigned long long &value) const {
  if(lookupValueLongOnly(name, value)) return true; // Reads values of the form 123L
  unsigned int int_value;
  if(lookupValue(name, int_value)) { // Reads normal integers
    value = (unsigned long long) int_value;
    return true;
  }
  return false;
}

bool CompoundConfigNode::lookupValue(const char *name, double &value) const {
  EXCEPTION_PROLOGUE;
  if (LNode) {
    int i_value = 0;
    if (LNode->lookupValue(name, i_value)) {
      value = static_cast<double>(i_value);
      return true;
    } else if (!LNode->lookupValue(name, value)) {
      std::string variableName;
      if (LNode->lookupValue(name, variableName) &&
          cConfig->getVariableRoot().exists(variableName) ) {
        return cConfig->getVariableRoot().lookupValue(variableName, value);
      } else return false;
    } else return true;
  }
  else if (YNode) {
    if (YNode.IsScalar() || !YNode[name].IsDefined() || !YNode[name].IsScalar()) return false;
    try {
      value = YNode[name].as<double, int>(0);
    } catch (YAML::BadConversion& e) {
      std::string variableName = YNode[name].as<std::string>();
      if (cConfig->getVariableRoot().exists(variableName)) {
        cConfig->getVariableRoot().lookupValue(variableName, value);
      } else {
        std::cerr << "Cannot find " << variableName << " for " << name << " under root key: variables" << std::endl;
        throw e;
      }
    }
    return true;
  }
  else {
    assert(false);
    return false;
  }
  EXCEPTION_EPILOGUE;
}

bool CompoundConfigNode::lookupValue(const char *name, float &value) const {
  EXCEPTION_PROLOGUE;
  if (LNode) {
    int i_value = 0;
    if (LNode->lookupValue(name, i_value)) {
      value = static_cast<float>(i_value);
      return true;
    } else if (!LNode->lookupValue(name, value)) {
      std::string variableName;
      if (LNode->lookupValue(name, variableName) &&
          cConfig->getVariableRoot().exists(variableName) ) {
        return cConfig->getVariableRoot().lookupValue(variableName, value);
      } else return false;
    } else return true;
  }
  else if (YNode) {
    if (YNode.IsScalar() || !YNode[name].IsDefined() || !YNode[name].IsScalar()) return false;
    try {
      value = YNode[name].as<float, int>(0);
    } catch (YAML::BadConversion& e) {
      std::string variableName = YNode[name].as<std::string>();
      if (cConfig->getVariableRoot().exists(variableName)) {
        cConfig->getVariableRoot().lookupValue(variableName, value);
      } else {
        std::cerr << "Cannot find " << variableName << " for " << name << " under root key: variables" << std::endl;
        throw e;
      }
    }
    return true;
  }
  else {
    assert(false);
    return false;
  }
  EXCEPTION_EPILOGUE;
}

bool CompoundConfigNode::lookupValue(const char *name, const char *&value) const {
  EXCEPTION_PROLOGUE;
  if (LNode) {
    if (LNode->lookupValue(name, value)) {
      std::string variableName(value);
      if (cConfig->getVariableRoot().exists(variableName)) {
        cConfig->getVariableRoot().lookupValue(variableName, value);
      }
      return true;
    } else return false;
  }
  else if (YNode) {
    if (YNode.IsScalar() || !YNode[name].IsDefined() || !YNode[name].IsScalar()) return false;
    value = YNode[name].as<std::string>().c_str();
    std::string variableName = YNode[name].as<std::string>();
    if (cConfig->getVariableRoot().exists(variableName)) {
      cConfig->getVariableRoot().lookupValue(variableName, value);
    }
    return true;
  }
  else {
    assert(false);
    return false;
  }
  EXCEPTION_EPILOGUE;
}

bool CompoundConfigNode::lookupValue(const char *name, std::string &value) const {
  EXCEPTION_PROLOGUE;
  if (LNode) {
    if (LNode->lookupValue(name, value)) {
      std::string variableName(value);
      if (cConfig->getVariableRoot().exists(variableName)) {
        cConfig->getVariableRoot().lookupValue(variableName, value);
      }
      return true;
    } else return false;
  }
  else if (YNode) {
    if (YNode.IsScalar() || !YNode[name].IsDefined() || !YNode[name].IsScalar()) return false;
    value = YNode[name].as<std::string>();
    std::string variableName = YNode[name].as<std::string>();
    if (cConfig->getVariableRoot().exists(variableName)) {
      cConfig->getVariableRoot().lookupValue(variableName, value);
    }
    return true;
  }
  else {
    assert(false);
    return false;
  }
  EXCEPTION_EPILOGUE;
}

/**
 * Resolves the current YNode into a string.
 * 
 * @return The current YNode as a string.
 */
std::string CompoundConfigNode::resolve() const {
  return YNode.as<std::string>();
}

/**
 * Sets the value at a given key to YAML::Null, instantiating it.
 * 
 * @param name  The key we in the Map we want to set to Null.
 * 
 * @return      Whether the setting was successful.
 * @post        If return is true the key provided is instantiated.
 */
bool CompoundConfigNode::instantiateKey(const char *name) {
  EXCEPTION_PROLOGUE;

  // Ensures the YNode is defined and is a Map or a Null which can become a Map.
  if (YNode && !YNode[name].IsDefined() && (YNode.IsMap() || YNode.IsNull()))
  {
    // Creates a Null YNode and assigns Null.
    YNode[name] = YAML::Null;
    return true;

  // Otherwise, cannot proceed with operation.
  } else
  {
    return false;
  }

  EXCEPTION_EPILOGUE;
}

/**
 * Sets the node to a Scalar value.
 * 
 * This is made in a template format for standardization across all Scalar types
 * to reduce the amount of code that needs to be changed upon refactor. In order
 * to avoid linker issues, please add an explicit instantiation at the bottom of
 * the file in order to avoid linker issues.
 * 
 * @tparam T      The C++ type of the Scalar we wish to set.
 * 
 * @param scalar  The Scalar we wish to set.
 * 
 * @return        Whether or not the scalar we wanted to set was set.
 * @post          If return is true the set was successful. If return is false,
 *                the value at the node was not replaced.
 */
template <typename T>
bool CompoundConfigNode::setScalar(const T scalar) {
  EXCEPTION_PROLOGUE;

  // Ensures the YNode is defined
  if (YNode)
  {
    YNode = scalar;
    return true;
  // If it is not defined, return false.
  } else
  {
    return false;
  }
  EXCEPTION_EPILOGUE;
}

/**
 * Sets a Node to another node.
 * 
 * @param node The node you want to set here.
 * 
 */
bool CompoundConfigNode::set(CompoundConfigNode& node)
{
  EXCEPTION_PROLOGUE;
  
  // Sets our YNode to that of the other node.
  YNode = node.getYNode();
  return true;

  EXCEPTION_EPILOGUE;
}

/**
 * Appends a value onto node.
 * 
 * @tparam T    The C++ type of the value we're attempting to append.
 * 
 * @param value The value we're trying to push on the vector.
 * 
 * @return      Whether we successfully pushed the value onto the vector.
 * @post        If we return true, we successfully pushed the vector onto the
 *              stack and converted it to a Sequence. If false, we modified
 *              nothing.
 */
template <typename T>
bool CompoundConfigNode::push_back(const T value) {
  EXCEPTION_PROLOGUE;

  // Ensures we can actually create a Sequence here
  if (!YNode || YNode.IsSequence() || YNode.IsNull())
  {
    YNode.push_back(value);
    return true;
  } else
  {
    return false;
  }

  EXCEPTION_EPILOGUE;
}

bool CompoundConfigNode::exists(const char *name) const {
  EXCEPTION_PROLOGUE;
  if (LNode) return LNode->exists(name);
  else if (YNode) return !YNode.IsScalar() && YNode[name].IsDefined();
  else {
    assert(false);
    return false;
  }
  EXCEPTION_EPILOGUE;
}

bool CompoundConfigNode::lookupArrayValue(const char* name, std::vector<std::string> &vectorValue) const {
  EXCEPTION_PROLOGUE;
  if (LNode) {
    assert(LNode->lookup(name).isArray());
    for (const std::string m: LNode->lookup(name))
    {
      vectorValue.push_back(m);
    }
    return true;
  } else if (YNode) {
    assert(YNode[name].IsSequence());
    for (auto n : YNode[name]) {
      vectorValue.push_back(n.as<std::string>());
    }
    return true;
  } else {
    assert(false);
    return false;
  }
  EXCEPTION_EPILOGUE;
}

bool CompoundConfigNode::isList() const {
  if(LNode) return LNode->isList();
  else if (YNode) {
    if (YNode.IsSequence()) {
      if (YNode.size() == 0) return true;
      else return !YNode[0].IsScalar();
    } else {
      return false;
    }
  }
  else {
    assert(false);
    return false;
  }
}

bool CompoundConfigNode::isArray() const {
  if(LNode) return LNode->isArray();
  else if (YNode) return YNode.IsSequence() && YNode[0].IsScalar();
  else {
    assert(false);
    return false;
  }
}

bool CompoundConfigNode::isMap() const {
  if(LNode) return LNode->isGroup();
  else if (YNode) return YNode.IsMap();
  else {
    assert(false);
    return false;
  }
}


int CompoundConfigNode::getLength() const {
  if(LNode) return LNode->getLength();
  else if (YNode) return YNode.size();
  else {
    assert(false);
    return false;
  }
}

CompoundConfigNode CompoundConfigNode::operator [](int idx) const {
  assert(isList() || isArray());

  if(LNode) return CompoundConfigNode(&(*LNode)[idx], YAML::Node(), cConfig);
  else if (YNode) {
    auto yIter = YNode.begin();
    for (int i = 0; i < idx; i++) yIter++;
    auto nextNode = *yIter;
    return CompoundConfigNode(nullptr, nextNode, cConfig);
  }
  else {
    assert(false);
    return CompoundConfigNode(nullptr, YAML::Node(), cConfig);
  }
}

bool CompoundConfigNode::getArrayValue(std::vector<std::string> &vectorValue) const {
  if (LNode) {
    assert(isArray());
    for (const std::string m: *LNode)
    {
      vectorValue.push_back(m);
    }
    return true;
  } else if (YNode) {
    assert(isArray());
    for (auto n : YNode)
    {
      vectorValue.push_back(n.as<std::string>());
    }
    return true;
  } else {
    assert(false);
    return false;
  }
}

bool CompoundConfigNode::getMapKeys(std::vector<std::string> &mapKeys) const {
  if (LNode) {
    assert(LNode->isGroup());
    for (auto it = LNode->begin(); it != LNode->end(); it++) {
      mapKeys.push_back(std::string(it->getName()));
    }
    return true;
  } else if (YNode) {
    assert(YNode.IsMap());
    for (auto it = YNode.begin(); it != YNode.end(); it++) {
      mapKeys.push_back(it->first.as<std::string>());
    }
    return true;
  } else {
    assert(false);
    return false;
  }
}



/* CompoundConfig */

CompoundConfig::CompoundConfig(const char* inputFile) {
  std::string contents = hyphens2underscores::hyphens2underscores_from_file(inputFile);

  if (std::strstr(inputFile, ".cfg")) {
    // LConfig.readFile(inputFile);
    LConfig.readString(contents);
    auto& lroot = LConfig.getRoot();
    useLConfig = true;
    root = CompoundConfigNode(&lroot, YAML::Node(), this);
  } else if (std::strstr(inputFile, ".yml") || std::strstr(inputFile, ".yaml")) {
    std::istringstream combinedStream(contents);
    YConfig = YAML::Load(combinedStream);
    root = CompoundConfigNode(nullptr, YConfig, this);
    useLConfig = false;
    // std::cout << YConfig << std::endl;
  } else {
    std::cerr << "ERROR: Input configuration file does not end with .cfg, .yml, or .yaml" << std::endl;
    exit(1);
  }
}

CompoundConfig::CompoundConfig(std::string input, std::string format) {
  // we only accept yaml version as a string input
  std::string contents = hyphens2underscores::hyphens2underscores(input);
  if (format.compare("cfg") == 0) {
    LConfig.readString(contents);
    auto& lroot = LConfig.getRoot();
    useLConfig = true;
    root = CompoundConfigNode(&lroot, YAML::Node(), this);
  } else if (format.compare("yml") == 0 || format.compare("yaml") == 0) {
    std::istringstream combinedStream(contents);
    YConfig = YAML::Load(combinedStream);
    root = CompoundConfigNode(nullptr, YConfig, this);
    useLConfig = false;
  } else {
    std::cerr << "ERROR: format should be one of the followings: cfg, yml, yaml." << std::endl;
    exit(1);
  }

  if (root.exists("variables")) {
    variableRoot = root.lookup("variables");
  } else {
    variableRoot = CompoundConfigNode(nullptr, YAML::Node()); // null node
  }
}

CompoundConfig::CompoundConfig(std::vector<std::string> inputFiles) {
  assert(inputFiles.size() > 0);
  inFiles = inputFiles;

  std::string combinedString;
  for (auto fName : inputFiles) {
    std::ifstream fin;
    fin.open(fName);
    std::string f((std::istreambuf_iterator<char>(fin)),
                   std::istreambuf_iterator<char>());
    combinedString += f;
    combinedString+= "\n"; // just to avoid files end with no newline
    fin.close();
  }
  
  if (std::strstr(inputFiles[0].c_str(), ".cfg")) {
    LConfig.readString(hyphens2underscores::hyphens2underscores(combinedString));
    auto& lroot = LConfig.getRoot();
    useLConfig = true;
    root = CompoundConfigNode(&lroot, YAML::Node(), this);
  } else if (std::strstr(inputFiles[0].c_str(), ".yml") || std::strstr(inputFiles[0].c_str(), ".yaml")) {
    std::istringstream combinedStream(hyphens2underscores::hyphens2underscores(combinedString));
    YConfig = YAML::Load(combinedStream);
    root = CompoundConfigNode(nullptr, YConfig, this);
    useLConfig = false;
    // std::cout << YConfig << std::endl;
  } else {
    std::cerr << "ERROR: Input configuration file does not end with .cfg, .yml, or .yaml" << std::endl;
    exit(1);
  }

  if (root.exists("variables")) {
    variableRoot = root.lookup("variables");
  } else {
    variableRoot = CompoundConfigNode(nullptr, YAML::Node()); // null node
  }
}

libconfig::Config& CompoundConfig::getLConfig() {
  return LConfig;
}

YAML::Node& CompoundConfig::getYConfig() {
  return YConfig;
}

CompoundConfigNode CompoundConfig::getRoot() const {
  return root;
}

CompoundConfigNode CompoundConfig::getVariableRoot() const {
  return variableRoot;
}

std::uint64_t parseElementSize(std::string name) {
  auto posBegin = name.find("[");
  auto posEnd = name.find("]");
  auto posDots = name.find("..");
  if (posBegin != std::string::npos && posEnd != std::string::npos && posDots != std::string::npos) {
    assert(posBegin < posEnd && posDots < posEnd && posBegin < posDots);
    auto beginIdx = name.substr(posBegin + 1, posDots - posBegin - 1);
    auto endIdx = name.substr(posDots + 2, posEnd - posDots - 2);
    return std::stoul(endIdx) - std::stoul(beginIdx) + 1;
  } else {
    return 1;
  }
}

std::string parseName(std::string name) {
  auto posStart = name.find("[");
  if (posStart != std::string::npos) {
    return name.substr(0, name.find("["));
  } else {
    return name;
  }
}

/*******************************************************************************
 * Explicit template instantiation.
 ******************************************************************************/
/* setScalar */
// Integer setters.
template bool CompoundConfigNode::setScalar(bool value);
template bool CompoundConfigNode::setScalar(int value);
template bool CompoundConfigNode::setScalar(unsigned int value);
// Long long setters.
template bool CompoundConfigNode::setScalar(long long value);
template bool CompoundConfigNode::setScalar(unsigned long long value);
// Floating point setters.
template bool CompoundConfigNode::setScalar(double value);
template bool CompoundConfigNode::setScalar(float value);
// String setters.
template bool CompoundConfigNode::setScalar(const char *value);
template bool CompoundConfigNode::setScalar(std::string value);
// Null setter.
template bool CompoundConfigNode::setScalar(YAML::_Null value);

/* push_back */
// Integer appending
template bool CompoundConfigNode::push_back(bool value);
template bool CompoundConfigNode::push_back(int value);
template bool CompoundConfigNode::push_back(unsigned int value);
// Long long appending
template bool CompoundConfigNode::push_back(long long value);
template bool CompoundConfigNode::push_back(unsigned long long value);
// Float appending
template bool CompoundConfigNode::push_back(double value);
template bool CompoundConfigNode::push_back(float value);
// String appending.
template bool CompoundConfigNode::push_back(const char *value);
template bool CompoundConfigNode::push_back(std::string value);
// YAML::Node appending
template bool CompoundConfigNode::push_back(YAML::Node value);
} // namespace config
