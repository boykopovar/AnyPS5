get_target_property(spirvToolsSourceDir SPIRV-Tools-opt SOURCE_DIR)
set(spirvToolsRedundancySource "${spirvToolsSourceDir}/redundancy_elimination.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${spirvToolsRedundancySource}")
file(READ "${spirvToolsRedundancySource}" spirvToolsRedundancy)

set(spirvToolsLocalRedundancySource "${spirvToolsSourceDir}/local_redundancy_elimination.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${spirvToolsLocalRedundancySource}")
file(READ "${spirvToolsLocalRedundancySource}" spirvToolsLocalRedundancy)
set(spirvToolsExpectedElimination [=[
  auto func = [this, &vnTable, &modified, value_to_ids](Instruction* inst) {
    if (inst->result_id() == 0) {
      return;
    }

    uint32_t value = vnTable.GetValueNumber(inst);

    if (value == 0) {
      return;
    }

    auto candidate = value_to_ids->insert({value, inst->result_id()});
    if (!candidate.second) {
      context()->KillNamesAndDecorates(inst);
      context()->ReplaceAllUsesWith(inst->result_id(), candidate.first->second);
      context()->KillInst(inst);
      modified = true;
    }
  };
  block->ForEachInst(func);
]=])
string(FIND "${spirvToolsLocalRedundancy}" "${spirvToolsExpectedElimination}" spirvToolsEliminationPosition)
if(spirvToolsEliminationPosition EQUAL -1)
    message(FATAL_ERROR "SPIRV-Tools value elimination has changed; review the local traversal fix")
endif()

set(spirvToolsOldTraversal [=[
  struct State {
    DominatorTreeNode* node;
    std::map<uint32_t, uint32_t> value_to_id_map;
  };
  std::vector<State> todo;
  todo.push_back({bb, std::map<uint32_t, uint32_t>()});
  bool modified = false;
  for (size_t next_node = 0; next_node < todo.size(); next_node++) {
    modified |= EliminateRedundanciesInBB(todo[next_node].node->bb_, vnTable,
                                          &todo[next_node].value_to_id_map);
    for (DominatorTreeNode* child : todo[next_node].node->children_) {
      todo.push_back({child, todo[next_node].value_to_id_map});
    }
  }
]=])
set(spirvToolsNewTraversal [=[
  struct State {
    DominatorTreeNode* node;
    size_t next_child;
    size_t added_begin;
    bool entered;
  };
  std::map<uint32_t, uint32_t> value_to_id_map;
  std::vector<uint32_t> added_values;
  std::vector<State> todo;
  todo.push_back({bb, 0, 0, false});
  bool modified = false;
  while (!todo.empty()) {
    auto& state = todo.back();
    if (!state.entered) {
      state.entered = true;
      state.node->bb_->ForEachInst([&](Instruction* inst) {
        if (inst->result_id() == 0) return;
        const uint32_t value = vnTable.GetValueNumber(inst);
        if (value == 0) return;
        auto candidate = value_to_id_map.insert({value, inst->result_id()});
        if (candidate.second) {
          added_values.push_back(value);
        } else {
          context()->KillNamesAndDecorates(inst);
          context()->ReplaceAllUsesWith(inst->result_id(), candidate.first->second);
          context()->KillInst(inst);
          modified = true;
        }
      });
    }
    const auto& children = state.node->children_;
    if (state.next_child != children.size()) {
      auto* child = children[state.next_child++];
      todo.push_back({child, 0, added_values.size(), false});
      continue;
    }
    while (added_values.size() != state.added_begin) {
      value_to_id_map.erase(added_values.back());
      added_values.pop_back();
    }
    todo.pop_back();
  }
]=])
string(FIND "${spirvToolsRedundancy}" "${spirvToolsOldTraversal}" spirvToolsTraversalPosition)
if(spirvToolsTraversalPosition EQUAL -1)
    message(FATAL_ERROR "SPIRV-Tools redundancy traversal has changed; review the local memory fix")
endif()
string(REPLACE "${spirvToolsOldTraversal}" "${spirvToolsNewTraversal}" spirvToolsRedundancy "${spirvToolsRedundancy}")

set(spirvToolsPatchedDir "${CMAKE_BINARY_DIR}/3rdparty/SPIRV-Tools/patched")
set(spirvToolsPatchedSource "${spirvToolsPatchedDir}/redundancy_elimination.cpp")
file(MAKE_DIRECTORY "${spirvToolsPatchedDir}")
file(CONFIGURE OUTPUT "${spirvToolsPatchedSource}" CONTENT "${spirvToolsRedundancy}" @ONLY)

get_target_property(spirvToolsSources SPIRV-Tools-opt SOURCES)
set(spirvToolsPatchedSources)
set(spirvToolsReplacements 0)
foreach(source IN LISTS spirvToolsSources)
    get_filename_component(sourcePath "${source}" ABSOLUTE BASE_DIR "${spirvToolsSourceDir}")
    if(sourcePath STREQUAL spirvToolsRedundancySource)
        list(APPEND spirvToolsPatchedSources "${spirvToolsPatchedSource}")
        math(EXPR spirvToolsReplacements "${spirvToolsReplacements} + 1")
    else()
        list(APPEND spirvToolsPatchedSources "${source}")
    endif()
endforeach()
if(NOT spirvToolsReplacements EQUAL 1)
    message(FATAL_ERROR "SPIRV-Tools optimizer sources have changed; review the local memory fix")
endif()
set_property(TARGET SPIRV-Tools-opt PROPERTY SOURCES "${spirvToolsPatchedSources}")

get_target_property(spirvToolsValidatorSourceDir SPIRV-Tools-static SOURCE_DIR)
set(spirvToolsValidatorSource "${spirvToolsValidatorSourceDir}/val/validate_id.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${spirvToolsValidatorSource}")
file(READ "${spirvToolsValidatorSource}" spirvToolsValidator)
string(FIND "${spirvToolsValidator}" "#include <unordered_set>" spirvToolsValidatorInclude)
if(spirvToolsValidatorInclude EQUAL -1)
    message(FATAL_ERROR "SPIRV-Tools validator includes have changed; review the local lifetime fix")
endif()
string(REPLACE "#include <unordered_set>" "#include <algorithm>\n#include <iterator>" spirvToolsValidator "${spirvToolsValidator}")
foreach(table IN ITEMS allow deny)
    set(oldDeclaration "static std::unordered_set<spv::Op> instruction_${table}_set{")
    set(newDeclaration "static constexpr spv::Op instruction_${table}_set[]{")
    set(oldLookup "instruction_${table}_set.count(opcode)")
    set(newLookup "(std::find(std::begin(instruction_${table}_set),\n                    std::end(instruction_${table}_set), opcode) !=\n          std::end(instruction_${table}_set))")
    string(FIND "${spirvToolsValidator}" "${oldDeclaration}" declarationPosition)
    string(FIND "${spirvToolsValidator}" "${oldLookup}" lookupPosition)
    if(declarationPosition EQUAL -1 OR lookupPosition EQUAL -1)
        message(FATAL_ERROR "SPIRV-Tools validator type-operand tables have changed; review the local lifetime fix")
    endif()
    string(REPLACE "${oldDeclaration}" "${newDeclaration}" spirvToolsValidator "${spirvToolsValidator}")
    string(REPLACE "${oldLookup}" "${newLookup}" spirvToolsValidator "${spirvToolsValidator}")
endforeach()
set(spirvToolsPatchedValidator "${spirvToolsPatchedDir}/validate_id.cpp")
file(CONFIGURE OUTPUT "${spirvToolsPatchedValidator}" CONTENT "${spirvToolsValidator}" @ONLY)
foreach(validatorTarget IN ITEMS SPIRV-Tools-static SPIRV-Tools-shared)
    if(NOT TARGET ${validatorTarget})
        continue()
    endif()
    get_target_property(validatorSources ${validatorTarget} SOURCES)
    set(patchedValidatorSources)
    set(validatorReplacements 0)
    foreach(source IN LISTS validatorSources)
        get_filename_component(sourcePath "${source}" ABSOLUTE BASE_DIR "${spirvToolsValidatorSourceDir}")
        if(sourcePath STREQUAL spirvToolsValidatorSource)
            list(APPEND patchedValidatorSources "${spirvToolsPatchedValidator}")
            math(EXPR validatorReplacements "${validatorReplacements} + 1")
        else()
            list(APPEND patchedValidatorSources "${source}")
        endif()
    endforeach()
    if(NOT validatorReplacements EQUAL 1)
        message(FATAL_ERROR "SPIRV-Tools validator sources have changed; review the local lifetime fix")
    endif()
    set_property(TARGET ${validatorTarget} PROPERTY SOURCES "${patchedValidatorSources}")
endforeach()
