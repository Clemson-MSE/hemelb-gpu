hemelb_dependency(parmetis find)

# category = IDX or REAL
function(METIS_TYPE category widths types output)
  list(LENGTH widths nw)
  list(LENGTH types nt)
  if(NOT (${nw} EQUAL ${nt}))
    message(FATAL_ERROR "Widths and types have different lengths")
  endif()

  list(APPEND CMAKE_REQUIRED_INCLUDES ${PARMETIS_INCLUDE_DIRS})
  math(EXPR loop_max "${nw} - 1")
  foreach(i RANGE ${loop_max})
    list(GET widths ${i} WIDTH)
    set(CMAKE_REQUIRED_FLAGS "-std=c++11")
    CHECK_CXX_SOURCE_COMPILES("#include <metis.h>
int main() {
  static_assert(${category}TYPEWIDTH==${WIDTH}, \"Not ${WIDTH} bit\");
}
" HAVE_METIS_${category}_${WIDTH})
    if(${HAVE_METIS_${category}_${WIDTH}})
      list(GET types ${i} TYPE)
      set(${output} ${TYPE} PARENT_SCOPE)
      return()
    endif()
  endforeach()
  message(FATAL_ERROR "Could not figure out metis ${category} width")
endfunction()

metis_type(IDX "32;64" "int32_t;int64_t" METIS_IDX_T)
metis_type(REAL "32;64" "float;double" METIS_REAL_T)

macro(hemelb_add_target_dependency_parmetis tgt)
  target_include_directories(${tgt} PRIVATE ${PARMETIS_INCLUDE_DIRS})
  target_link_libraries(${tgt} PRIVATE ${ZLIB_LIBRARIES})
endmacro()
