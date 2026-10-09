---
applyTo: "**/Tests/L1Tests/tests/*"
---

# WPEFramework Plugin L1 Unit Testing Guide

### L1 Testing Objectives

For specified APIs, ensure the generated L1 tests meet the following criteria:
- **Scenario Testing**: Normal operation, boundary conditions, and error cases must all be covered
- **Event Verification**: Correct event dispatch and notification flow must be validated
- **Isolation**: Tests must use mocks for external dependencies to ensure plugin logic is tested in isolation

## Testing
When generating L1 tests, please follow the below step-by-step approach:

1. **Checking for Adequate Context and Information**
    - Ensure that the test fixture, interface API header file, mock interfaces, and plugin implementation header files are attached by the user for your reference.

2. **Understanding the Plugin**
    - For each API/method, step through the implementation code line by line to:
        - Map all external dependencies it calls or uses
        - Identify key data structures it creates or manipulates
        - Document control flow paths (including conditions and error handling)
        - Note any system resources or services accessed
    - Before creating the tests, fully understand the event dispatch logic and notification flow.

3. **Test Coverage**
    - For every event and notification method specified by the user or listed in the API header, generate at least one test case. Include both positive and negative inputs where applicable, and boundary tests for methods with boundary conditions.
    - Respect member and method access levels. If a member or method is private, access it indirectly through public APIs or test hooks if possible.
    - Do not summarize, skip, or instruct the user to extrapolate patterns. Every specified method must be tested explicitly.
    - For error handling, trigger and verify error cases for each method.

4. **Test Design**
    - Maximize code coverage by testing normal, boundary, and error cases for each method.
    - Use GTest syntax.
    - Write all tests as modifications to the existing test fixture file.
    - Do not include comments which only instruct the user to extrapolate or generate similar tests.
    - Every test must verify specific return values, state changes, or behaviors. 
    - Avoid GTest macros that only check for absence of exceptions (e.g., EXPECT_NO_THROW) without validating actual outcomes.
    - Use EXPECT_CALL for mocks with ::testing::_ for arguments unless a specific value is required for the test logic.
    - Use only error codes defined in the implementation (e.g., Core::ERROR_NONE, Core::ERROR_GENERAL, Core::ERROR_INVALID_PARAMETER). Do not invent error codes. Use the error codes as defined in the implementation.
    - Ensure all payloads and parameters match the expected types and formats as defined in the API and implementation headers.
    - Ensure mock objects are properly initialized and cleaned up in the test fixture setup and teardown.
    - Use EXPECT_TRUE, EXPECT_FALSE, and EXPECT_EQ to check returned, updated, or tested event values using the test fixture’s initialization, notification flags, handlers, and helper methods.

5. **Format**
    - Output only code, as if modifying the existing test fixture file.
    - Do not include explanations, summaries, or instructions.
    - Do not include comments like "repeat for other methods" or "similarly for X".
    - Do not include any code that is not a test for a specific method.

## Testing Example
Here is a snippet from the UserSettings plugin implementation and header files:

**UserSettingsImplementation.h**
```cpp
Core::hresult SetPresentationLanguage(const string& presentationLanguage) override;
```

**UserSettingsImplementation.cpp**
```cpp
uint32_t UserSettingsImplementation::SetUserSettingsValue(const string& key, const string& value)
{
    uint32_t status = Core::ERROR_GENERAL;
    _adminLock.Lock();

    LOGINFO("Key[%s] value[%s]", key.c_str(), value.c_str());
    if (nullptr != _remotStoreObject)
    {
        status = _remotStoreObject->SetValue(Exchange::IStore2::ScopeType::DEVICE, USERSETTINGS_NAMESPACE, key, value, 0);
    }
    else
    {
        LOGERR("_remotStoreObject is null");
    }
    _adminLock.Unlock();
    return status;
}

Core::hresult UserSettingsImplementation::SetPresentationLanguage(const string& presentationLanguage)
{
    uint32_t status = Core::ERROR_GENERAL;

    LOGINFO("presentationLanguage: %s", presentationLanguage.c_str());
    status = SetUserSettingsValue(USERSETTINGS_PRESENTATION_LANGUAGE_KEY, presentationLanguage);
    return status;
}
```

Here is are some example generated tests for the UserSettings plugin:

**test_UserSettings.cpp**
```cpp
TEST_F(UserSettingsTest, SetPresentationLanguage_Success)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setPresentationLanguage"), _T("{\"presentationLanguage\": \"en-US\"}"), response));
}

TEST_F(UserSettingsTest, SetPresentationLanguage_Failure)
{
    EXPECT_CALL(*p_store2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setPresentationLanguage"), _T("{\"presentationLanguage\": \"en-US\"}"), response));
}
```