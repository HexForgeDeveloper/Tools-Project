#include "Modules/ModuleManager.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CoreDelegates.h"
#include "Editor.h"
#include "WireBundleGesture.h"
#include "WireBundleMenus.h"
#include "WireBundleShape.h"

class FWireBundleModule final : public IModuleInterface
{
    TSharedPtr<IInputProcessor> Input;
    TSharedPtr<IInputProcessor> ShapeInput;
    FDelegateHandle PostEngineInitHandle;
    FDelegateHandle MenuHandle;
    void InitializeInput()
    {
        if (!Input && FSlateApplication::IsInitialized())
        {
            MenuHandle = WireBundle::RegisterMenus();
            ShapeInput = WireBundle::CreateShapeInput();
            FSlateApplication::Get().RegisterInputPreProcessor(ShapeInput);
            Input = WireBundle::CreateInput();
            FSlateApplication::Get().RegisterInputPreProcessor(Input);
        }
    }
public:
    virtual void StartupModule() override
    {
        // Serialized editor records must be registered before assets load.
        // Slate input is attached later, once the editor has initialized.
        PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FWireBundleModule::InitializeInput);
        if (GEditor) { InitializeInput(); }
    }
    virtual void ShutdownModule() override
    {
        FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
        if (Input && FSlateApplication::IsInitialized())
        { FSlateApplication::Get().UnregisterInputPreProcessor(Input); }
        if (ShapeInput && FSlateApplication::IsInitialized())
        { FSlateApplication::Get().UnregisterInputPreProcessor(ShapeInput); }
        ShapeInput.Reset();
        WireBundle::UnregisterMenus(MenuHandle);
        Input.Reset();
    }
};
IMPLEMENT_MODULE(FWireBundleModule, WireBundle)
