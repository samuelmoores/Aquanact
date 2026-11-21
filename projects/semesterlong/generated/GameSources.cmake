if(NOT AQUANACT_PROJECT_ROOT)
    get_filename_component(AQUANACT_GAME_CODE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
else()
    set(AQUANACT_GAME_CODE_ROOT "${AQUANACT_PROJECT_ROOT}")
endif()

set(GAME_SOURCES
    "${AQUANACT_GAME_CODE_ROOT}/src/AIController.cpp"
    "${AQUANACT_GAME_CODE_ROOT}/src/AttackSphere.cpp"
    "${AQUANACT_GAME_CODE_ROOT}/src/GameManager.cpp"
    "${AQUANACT_GAME_CODE_ROOT}/src/Health.cpp"
    "${AQUANACT_GAME_CODE_ROOT}/src/PlayerAttack.cpp"
    "${AQUANACT_GAME_CODE_ROOT}/src/PlayerCombat.cpp"
    "${AQUANACT_GAME_CODE_ROOT}/src/PlayerController.cpp"
    "${AQUANACT_GAME_CODE_ROOT}/src/PlayerHealth.cpp"
    "${AQUANACT_GAME_CODE_ROOT}/src/TestClass.cpp"
    "${AQUANACT_GAME_CODE_ROOT}/src/TriggerSphereBedroomSecondwall.cpp"
    "${AQUANACT_GAME_CODE_ROOT}/src/TriggerSphereDresser.cpp"
    "${AQUANACT_GAME_CODE_ROOT}/src/ComponentRegistry.cpp"
)
