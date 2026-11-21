#include "TestClass.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/InputManager.h"
#include "Engine/Core/Root.h"

void TestClass::startUp(Entity&)
{
}

// Keep component metadata in the header with the binding/event list macros.
// Add runtime logic here only if the component needs it.
