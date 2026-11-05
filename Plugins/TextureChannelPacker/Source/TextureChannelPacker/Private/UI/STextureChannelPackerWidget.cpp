// Copyright 2025 DimAlek. All Rights Reserved.

#include "UI/STextureChannelPackerWidget.h"
#include "UI/TextureChannelPackerUIConversions.h"
#include "ContentBrowserModule.h"
#include "Engine/Texture.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IContentBrowserSingleton.h"
#include "PropertyCustomizationHelpers.h"
#include "Services/TextureChannelPackerPathService.h"
#include "Services/TextureChannelPackerPatternService.h"
#include "Services/TextureChannelPackerTextureService.h"
#include "Settings/TextureChannelPackerSettings.h"
#include "Styling/AppStyle.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "STextureChannelPackerWidget"

void STextureChannelPackerWidget::Construct(const FArguments& InArgs)
{
	OutputSettings = UTextureChannelPackerSettings::Get()->Presets.Num() > 0
		? UTextureChannelPackerSettings::Get()->Presets[0].OutputSettings
		: FTextureChannelPackerOutputSettings();

	InitializeOptions();
	RefreshPresetOptions();

	ChannelRows.SetNum(4);
	for (int32 Index = 0; Index < ChannelRows.Num(); ++Index)
	{
		ChannelRows[Index].OutputChannel = static_cast<ETextureChannelPackerChannel>(Index);
		ChannelRows[Index].SourceChannel = static_cast<ETextureChannelPackerChannel>(Index);
		ChannelRows[Index].bUseConstant = Index == 3;
		ChannelRows[Index].ConstantValue = Index == 3 ? 255 : 0;
	}

	UnpackOutputs = {
		{ ETextureChannelPackerChannel::Red, TEXT("_R"), true },
		{ ETextureChannelPackerChannel::Green, TEXT("_G"), true },
		{ ETextureChannelPackerChannel::Blue, TEXT("_B"), true },
		{ ETextureChannelPackerChannel::Alpha, TEXT("_A"), false },
	};

	if (UTextureChannelPackerSettings::Get()->Presets.Num() > 0)
	{
		ApplyPreset(UTextureChannelPackerSettings::Get()->Presets[0]);
	}

	/* clang-format off */
	ChildSlot
		[
			SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
				.Padding(8.0f)
				[
					SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 6.0f)
						[
							SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("Brushes.Header"))
								.Padding(FMargin(8.0f, 5.0f))
								[
									SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										.FillWidth(1.0f)
										.VAlign(VAlign_Center)
										[
											SNew(STextBlock)
												.Text(LOCTEXT("WindowHeader", "Texture Packer"))
												.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
										]

										+ SHorizontalBox::Slot()
										.AutoWidth()
										.VAlign(VAlign_Center)
										[
											BuildToolbar()
										]
								]
						]

						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(SBox)
										.WidthOverride(330.0f)
										[
											SNew(SBorder)
												.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
												.Padding(10.0f)
												[
													SNew(SVerticalBox)
														+ SVerticalBox::Slot()
														.AutoHeight()
														[
															BuildActionPanel()
														]

														+ SVerticalBox::Slot()
														.AutoHeight()
														.Padding(0.0f, 10.0f)
														[
															SNew(SSeparator)
														]

														+ SVerticalBox::Slot()
														.FillHeight(1.0f)
														[
															BuildPreviewPanel()
														]
												]
										]
								]

								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								[
									SNew(SBorder)
										.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
										.Padding(10.0f)
										[
											SNew(SScrollBox)
												+ SScrollBox::Slot()
												[
													SNew(SVerticalBox)
														+ SVerticalBox::Slot()
														.AutoHeight()
														.Padding(0.0f, 0.0f, 0.0f, 8.0f)
														[
															SNew(STextBlock)
																.Text(LOCTEXT("OperationsHeader", "Operations"))
																.Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
														]
														+ SVerticalBox::Slot()
														.AutoHeight()
														[
															SAssignNew(PackMappingsPanel, SBox)
																[
																	BuildPackMappings()
																]
														]

														+ SVerticalBox::Slot()
														.AutoHeight()
														[
															SAssignNew(UnpackSettingsPanel, SBox)
																[
																	BuildUnpackSettings()
																]
														]
														
														+ SVerticalBox::Slot()
														.AutoHeight()
														.Padding(0.0f, 12.0f)
														[
															SNew(SSeparator)
														]

														+ SVerticalBox::Slot()
														.AutoHeight()
														[
															BuildOutputSettings()
														]
												]
										]
								]
						]
				]
		];
	/* clang-format on */
}

void STextureChannelPackerWidget::InitializeOptions()
{
	OperationOptions = {
		MakeShared<FString>(TEXT("Pack")),
		MakeShared<FString>(TEXT("Repack")),
		MakeShared<FString>(TEXT("Unpack")),
		MakeShared<FString>(TEXT("Copy Channels")),
	};
	CurrentOperationOption = OperationOptions[0];

	ChannelOptions = {
		MakeShared<FString>(TEXT("R")),
		MakeShared<FString>(TEXT("G")),
		MakeShared<FString>(TEXT("B")),
		MakeShared<FString>(TEXT("A")),
	};

	CompressionOptions = {
		MakeShared<FString>(TEXT("Masks")),
		MakeShared<FString>(TEXT("Grayscale")),
		MakeShared<FString>(TEXT("Default")),
		MakeShared<FString>(TEXT("HDR")),
		MakeShared<FString>(TEXT("Vector Displacement")),
	};
	CurrentCompressionOption = CompressionOptions[0];

	MipOptions = {
		MakeShared<FString>(TEXT("From Texture Group")),
		MakeShared<FString>(TEXT("No Mipmaps")),
		MakeShared<FString>(TEXT("Sharpen 0")),
		MakeShared<FString>(TEXT("Sharpen 4")),
	};
	CurrentMipOption = MipOptions[0];

	FilterOptions = {
		MakeShared<FString>(TEXT("Default")),
		MakeShared<FString>(TEXT("Nearest")),
		MakeShared<FString>(TEXT("Bilinear")),
		MakeShared<FString>(TEXT("Trilinear")),
	};
	CurrentFilterOption = FilterOptions[0];

	LODGroupOptions = {
		MakeShared<FString>(TEXT("World")),
		MakeShared<FString>(TEXT("World Specular")),
		MakeShared<FString>(TEXT("World Normal Map")),
		MakeShared<FString>(TEXT("UI")),
		MakeShared<FString>(TEXT("Pixels 2D")),
	};
	CurrentLODGroupOption = LODGroupOptions[0];
}

void STextureChannelPackerWidget::RefreshPresetOptions()
{
	PresetOptions.Reset();
	for (const FTextureChannelPackerPreset& Preset : UTextureChannelPackerSettings::Get()->Presets)
	{
		PresetOptions.Add(MakeShared<FString>(Preset.Name));
	}

	CurrentPresetOption = PresetOptions.Num() > 0 ? PresetOptions[0] : nullptr;
}

TSharedRef<SWidget> STextureChannelPackerWidget::BuildToolbar()
{
	/* clang-format off */
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(STextBlock)
				.Text(LOCTEXT("OperationLabel", "Mode"))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 14.0f, 0.0f)
		[
			SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&OperationOptions)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
					{
						return SNew(STextBlock).Text(Item.IsValid() ? FText::FromString(*Item) : FText::GetEmpty());
					})
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
					{
						InvalidatePreview();
						if (NewSelection.IsValid())
						{
							CurrentOperationOption = NewSelection;
							Operation = FTextureChannelPackerUIConversions::StringToOperation(*NewSelection);
							RefreshDynamicPanels();
						}
					})
				[
					SNew(STextBlock).Text(this, &STextureChannelPackerWidget::GetOperationText)
				]
		]
	+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(STextBlock)
				.Text(LOCTEXT("PresetLabel", "Preset"))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&PresetOptions)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
					{
						return SNew(STextBlock).Text(Item.IsValid() ? FText::FromString(*Item) : FText::GetEmpty());
					})
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
					{
						InvalidatePreview();
						if (NewSelection.IsValid())
						{
							CurrentPresetOption = NewSelection;
							if (const FTextureChannelPackerPreset* Preset = UTextureChannelPackerSettings::Get()->FindPreset(*NewSelection))
							{
								ApplyPreset(*Preset);
							}
						}
					})
				[
					SNew(STextBlock).Text(this, &STextureChannelPackerWidget::GetPresetText)
				]
		];
	/* clang-format off */
}

TSharedRef<SWidget> STextureChannelPackerWidget::BuildActionPanel()
{
	/* clang-format off */
	const auto BuildSidebarButton = [](const FText& Label, const FName IconName, const FOnClicked& OnClicked, const FSlateColor& ButtonColor)
		{
			return SNew(SButton)
				.HAlign(HAlign_Fill)
				.ContentPadding(FMargin(10.0f, 7.0f))
				.ButtonColorAndOpacity(ButtonColor)
				.OnClicked(OnClicked)
				[
					SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 7.0f, 0.0f)
						[
							SNew(SImage)
								.Image(FAppStyle::GetBrush(IconName))
								.ColorAndOpacity(FSlateColor::UseForeground())
						]

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(Label)
								.Justification(ETextJustify::Center)
						]
				];
		};

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			SNew(STextBlock)
				.Text(LOCTEXT("ActionsHeader", "Actions"))
				.Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSidebarButton(
				LOCTEXT("GenerateButton", "Generate"),
				TEXT("Icons.Check"),
				FOnClicked::CreateSP(this, &STextureChannelPackerWidget::OnGenerateClicked),
				FSlateColor(FLinearColor(0.18f, 0.32f, 0.22f, 1.0f)))
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSidebarButton(
				LOCTEXT("PreviewButton", "Preview"),
				TEXT("Icons.Visibility"),
				FOnClicked::CreateSP(this, &STextureChannelPackerWidget::OnPreviewClicked),
				FSlateColor(FLinearColor(0.18f, 0.26f, 0.40f, 1.0f)))
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildSidebarButton(
				LOCTEXT("UseSelectedButton", "Use Selected"),
				TEXT("Icons.Search"),
				FOnClicked::CreateLambda([this]()
					{
						ApplySelectedContentBrowserTextures();
						return FReply::Handled();
					}),
				FSlateColor(FLinearColor(0.20f, 0.20f, 0.20f, 1.0f)))
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		[
			BuildSidebarButton(
				LOCTEXT("SavePresetButton", "Save Preset"),
				TEXT("Icons.Save"),
				FOnClicked::CreateSP(this, &STextureChannelPackerWidget::OnSavePresetClicked),
				FSlateColor(FLinearColor(0.20f, 0.20f, 0.20f, 1.0f)))
		];
	/* clang-format on */
}

TSharedRef<SWidget> STextureChannelPackerWidget::BuildOutputSettings()
{
	/* clang-format off */
	TSharedRef<SComboButton> PathPickerComboButton = SNew(SComboButton)
		.ContentPadding(FMargin(2.0f))
		.ButtonContent()
		[
			SNew(SImage).Image(FAppStyle::GetBrush("Icons.FolderClosed"))
		];

	TWeakPtr<SComboButton> WeakComboButton = PathPickerComboButton;
	PathPickerComboButton->SetOnGetMenuContent(FOnGetContent::CreateLambda([this, WeakComboButton]()
		{
			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			FPathPickerConfig PathPickerConfig;
			PathPickerConfig.DefaultPath = OutputPackagePath;
			PathPickerConfig.OnPathSelected = FOnPathSelected::CreateLambda([this, WeakComboButton](const FString& NewPath)
				{
					OutputPackagePath = NewPath;
					if (TSharedPtr<SComboButton> ComboButton = WeakComboButton.Pin())
					{
						ComboButton->SetIsOpen(false);
					}
				});

			return ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig);
		}));

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(STextBlock)
				.Text(LOCTEXT("OutputSettingsHeader", "Output"))
				.Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(STextBlock)
				.Text(LOCTEXT("OutputPathShortLabel", "Path"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 5.0f, 0.0f)
				[
					SNew(SEditableTextBox)
						.Text_Lambda([this]() { return FText::FromString(OutputPackagePath); })
						.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type)
							{
								// Preserve invalid input so generation reports it rather than silently changing the destination.
								const FString EnteredPath = NewText.ToString();
								const FString NormalizedPath = FTextureChannelPackerPathService::NormalizeOutputPath(EnteredPath);
								OutputPackagePath = NormalizedPath.IsEmpty() ? EnteredPath : NormalizedPath;
							})
				]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					PathPickerComboButton
				]
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(STextBlock)
				.Text(LOCTEXT("AssetNameShortLabel", "Asset Name"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(SEditableTextBox)
				.Text_Lambda([this]() { return FText::FromString(OutputBaseName); })
				.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type CommitType)
					{
						OutputBaseName = FTextureChannelPackerPathService::SanitizeAssetName(NewText.ToString());
						if (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus)
						{
							bOutputNameManuallyEdited = true;
						}
					})
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SCheckBox)
						.IsChecked_Lambda([this]() { return OutputSettings.bUseFirstInputResolution ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
							{
								InvalidatePreview();
								OutputSettings.bUseFirstInputResolution = NewState == ECheckBoxState::Checked;
							})
						[
							SNew(STextBlock).Text(LOCTEXT("UseFirstInputSize", "Use input size"))
						]
				]
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(STextBlock)
				.Text(LOCTEXT("ResolutionShortLabel", "Resolution"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(SNumericEntryBox<int32>)
				.Value_Lambda([this]() { return OutputSettings.Resolution; })
				.IsEnabled_Lambda([this]() { return !OutputSettings.bUseFirstInputResolution; })
				.MinValue(1)
				.MaxValue(16384)
				.OnValueChanged_Lambda([this](const int32 NewValue)
					{
						InvalidatePreview();
						OutputSettings.Resolution = NewValue;
					})
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(STextBlock).Text(LOCTEXT("CompressionLabel", "Compression"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&CompressionOptions)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) { return SNew(STextBlock).Text(Item.IsValid() ? FText::FromString(*Item) : FText::GetEmpty()); })
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
					{
						InvalidatePreview();
						if (NewSelection.IsValid())
						{
							CurrentCompressionOption = NewSelection;
							OutputSettings.CompressionSettings = FTextureChannelPackerUIConversions::StringToCompression(*NewSelection);
						}
					})
				[
					SNew(STextBlock).Text_Lambda([this]() { return CurrentCompressionOption.IsValid() ? FText::FromString(*CurrentCompressionOption) : FText::GetEmpty(); })
				]
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SCheckBox)
						.IsChecked_Lambda([this]() { return OutputSettings.bSRGB ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
							{
								InvalidatePreview();
								OutputSettings.bSRGB = NewState == ECheckBoxState::Checked;
							})
						[
							SNew(STextBlock).Text(LOCTEXT("SRGBLabel", "sRGB"))
						]
				]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SCheckBox)
						.IsChecked_Lambda([this]() { return OutputSettings.bVirtualTextureStreaming ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
							{
								InvalidatePreview();
								OutputSettings.bVirtualTextureStreaming = NewState == ECheckBoxState::Checked;
							})
						[
							SNew(STextBlock).Text(LOCTEXT("VirtualTextureLabel", "Virtual Texture"))
						]
				]
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(STextBlock).Text(LOCTEXT("MipLabel", "Mip Maps"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&MipOptions)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) { return SNew(STextBlock).Text(Item.IsValid() ? FText::FromString(*Item) : FText::GetEmpty()); })
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
					{
						InvalidatePreview();
						if (NewSelection.IsValid())
						{
							CurrentMipOption = NewSelection;
							OutputSettings.MipGenSettings = FTextureChannelPackerUIConversions::StringToMip(*NewSelection);
						}
					})
				[
					SNew(STextBlock).Text_Lambda([this]() { return CurrentMipOption.IsValid() ? FText::FromString(*CurrentMipOption) : FText::GetEmpty(); })
				]
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(STextBlock).Text(LOCTEXT("FilterLabel", "Filter"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&FilterOptions)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) { return SNew(STextBlock).Text(Item.IsValid() ? FText::FromString(*Item) : FText::GetEmpty()); })
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
					{
						InvalidatePreview();
						if (NewSelection.IsValid())
						{
							CurrentFilterOption = NewSelection;
							OutputSettings.Filter = FTextureChannelPackerUIConversions::StringToFilter(*NewSelection);
						}
					})
				[
					SNew(STextBlock).Text_Lambda([this]() { return CurrentFilterOption.IsValid() ? FText::FromString(*CurrentFilterOption) : FText::GetEmpty(); })
				]
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(STextBlock).Text(LOCTEXT("LODGroupLabel", "LOD Group"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&LODGroupOptions)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) { return SNew(STextBlock).Text(Item.IsValid() ? FText::FromString(*Item) : FText::GetEmpty()); })
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
					{
						InvalidatePreview();
						if (NewSelection.IsValid())
						{
							CurrentLODGroupOption = NewSelection;
							OutputSettings.LODGroup = FTextureChannelPackerUIConversions::StringToLODGroup(*NewSelection);
						}
					})
				[
					SNew(STextBlock).Text_Lambda([this]() { return CurrentLODGroupOption.IsValid() ? FText::FromString(*CurrentLODGroupOption) : FText::GetEmpty(); })
				]
		];
	/* clang-format on */
}

TSharedRef<SWidget> STextureChannelPackerWidget::BuildPreviewPanel()
{
	/* clang-format off */
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(STextBlock)
				.Text(LOCTEXT("PreviewHeader", "Preview"))
				.Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(SBox)
				.MinDesiredHeight(64.0f)
				[
					SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
						.Padding(4.0f)
						[
							SNew(SOverlay)
								+ SOverlay::Slot()
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Fill)
								[
									SAssignNew(PreviewScaleBox, SScaleBox)
										.Stretch(EStretch::ScaleToFit)
										.StretchDirection(EStretchDirection::Both)
										.Visibility(this, &STextureChannelPackerWidget::GetPreviewImageVisibility)
										[
											SAssignNew(PreviewImage, SImage)
												.Image(&PreviewBrush)
										]
								]
							+ SOverlay::Slot()
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.Text(LOCTEXT("NoPreviewPlaceholder", "No Preview"))
										.ColorAndOpacity(FSlateColor::UseSubduedForeground())
										.Visibility(this, &STextureChannelPackerWidget::GetPreviewPlaceholderVisibility)
								]
						]
				]
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 3.0f, 0.0f)
				[
					BuildPreviewModeButton(ETextureChannelPackerPreviewMode::Composite, LOCTEXT("PreviewComposite", "RGB"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 3.0f, 0.0f)
				[
					BuildPreviewModeButton(ETextureChannelPackerPreviewMode::Red, LOCTEXT("PreviewRed", "R"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 3.0f, 0.0f)
				[
					BuildPreviewModeButton(ETextureChannelPackerPreviewMode::Green, LOCTEXT("PreviewGreen", "G"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 3.0f, 0.0f)
				[
					BuildPreviewModeButton(ETextureChannelPackerPreviewMode::Blue, LOCTEXT("PreviewBlue", "B"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					BuildPreviewModeButton(ETextureChannelPackerPreviewMode::Alpha, LOCTEXT("PreviewAlpha", "A"))
				]
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
				.MaxDesiredHeight(96.0f)
				[
					SAssignNew(PreviewStatusScroll, SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(PreviewStatusLabel, STextBlock)
							.Text(this, &STextureChannelPackerWidget::GetPreviewStatusText)
							.ToolTipText(this, &STextureChannelPackerWidget::GetPreviewStatusText)
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							.WrapTextAt(286.0f)
					]
				]
		];

}

TSharedRef<SWidget> STextureChannelPackerWidget::BuildPreviewModeButton(const ETextureChannelPackerPreviewMode Mode, const FText& Label)
{
	return SNew(SButton)
		.HAlign(HAlign_Center)
		.ContentPadding(FMargin(7.0f, 3.0f))
		.ButtonColorAndOpacity(this, &STextureChannelPackerWidget::GetPreviewModeButtonColor, Mode)
		.OnClicked(FOnClicked::CreateSP(this, &STextureChannelPackerWidget::OnPreviewModeClicked, Mode))
		[
			SNew(STextBlock).Text(Label)
		];
}

FReply STextureChannelPackerWidget::OnPreviewModeClicked(const ETextureChannelPackerPreviewMode Mode)
{
	PreviewMode = Mode;
	if (PreviewTexture.Get())
	{
		RefreshPreview();
	}
	return FReply::Handled();
}

TSharedRef<SWidget> STextureChannelPackerWidget::BuildPackMappings()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		.Visibility(this, &STextureChannelPackerWidget::GetPackVisibility)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			SNew(STextBlock)
				.Text(LOCTEXT("ChannelMappingsHeader", "Mappings"))
				.Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
		];

	for (int32 Index = 0; Index < ChannelRows.Num(); ++Index)
	{
		const int32 RowIndex = Index;
		Box->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(STextBlock)
							.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &STextureChannelPackerWidget::GetOutputChannelText, RowIndex)))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0.0f, 0.0f, 5.0f, 0.0f)
					[
						BuildChannelRowTexturePicker(RowIndex)
					]

					+SHorizontalBox::Slot()
					[
						SNew(SVerticalBox)
							+SVerticalBox::Slot	()
							.AutoHeight()
							.Padding(0.0f, 5.0f)
							[
								SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.Padding(0.0f, 0.0f, 5.0f, 0.0f)
									[
										BuildChannelCombo(RowIndex)
									]
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.Padding(0.0f, 0.0f, 5.0f, 0.0f)
									[
										SNew(SCheckBox)
											.IsChecked(TAttribute<ECheckBoxState>::Create(TAttribute<ECheckBoxState>::FGetter::CreateSP(this, &STextureChannelPackerWidget::GetUseConstantState, RowIndex)))
											.OnCheckStateChanged(FOnCheckStateChanged::CreateSP(this, & STextureChannelPackerWidget::OnUseConstantChanged, RowIndex))
											[
												SNew(STextBlock).Text(LOCTEXT("ConstantLabel", "Const"))
											]
									]

									+ SHorizontalBox::Slot()
									.AutoWidth()
									[
										SNew(SBox)
											.HeightOverride(8.0f)
											[
												SNew(SSpinBox<int32>)
													.MinValue(0)
													.MaxValue(255)
													.Value_Lambda([this, RowIndex]()
														{
															return ChannelRows.IsValidIndex(RowIndex) ? ChannelRows[RowIndex].ConstantValue : 0;
														})
													.OnValueChanged_Lambda([this, RowIndex](const int32 NewValue)
														{
															InvalidatePreview();
															if (ChannelRows.IsValidIndex(RowIndex))
															{
																ChannelRows[RowIndex].ConstantValue = NewValue;
															}
														})
											]
									]
							]
					]
			];
	}

	return Box;
}

FText STextureChannelPackerWidget::GetOutputChannelText(int32 RowIndex) const
{
	return ChannelRows.IsValidIndex(RowIndex)
			? FText::FromString(FTextureChannelPackerUIConversions::ChannelToString(ChannelRows[RowIndex].OutputChannel))
			: FText::GetEmpty();
}

void STextureChannelPackerWidget::OnUseConstantChanged(ECheckBoxState NewState, int32 RowIndex)
{
	if (ChannelRows.IsValidIndex(RowIndex))
	{
		InvalidatePreview();
		ChannelRows[RowIndex].bUseConstant = (NewState == ECheckBoxState::Checked);
	}
}

ECheckBoxState STextureChannelPackerWidget::GetUseConstantState(int32 RowIndex) const
{
	return ChannelRows.IsValidIndex(RowIndex) && ChannelRows[RowIndex].bUseConstant
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

TSharedRef<SWidget> STextureChannelPackerWidget::BuildUnpackSettings()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		.Visibility(this, &STextureChannelPackerWidget::GetUnpackVisibility)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			SNew(STextBlock)
				.Text(LOCTEXT("UnpackHeader", "Unpack Source"))
				.Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			BuildTexturePicker(UnpackSourceTexture)
		];

	for (int32 Index = 0; Index < UnpackOutputs.Num(); ++Index)
	{
		const int32 OutputIndex = Index;
		Box->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 3.0f)
			[
				SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(SCheckBox)
							.IsChecked_Lambda([this, OutputIndex]()
								{
									return UnpackOutputs.IsValidIndex(OutputIndex) && UnpackOutputs[OutputIndex].bEnabled
										? ECheckBoxState::Checked
										: ECheckBoxState::Unchecked;
								})
							.OnCheckStateChanged_Lambda([this, OutputIndex](const ECheckBoxState NewState)
								{
									InvalidatePreview();
									if (UnpackOutputs.IsValidIndex(OutputIndex))
									{
										UnpackOutputs[OutputIndex].bEnabled = NewState == ECheckBoxState::Checked;
									}
								})
							[
								SNew(STextBlock).Text_Lambda([this, OutputIndex]()
									{
										return UnpackOutputs.IsValidIndex(OutputIndex)
											? FText::Format(LOCTEXT("UnpackChannelLabel", "Extract {0}"), FText::FromString(FTextureChannelPackerUIConversions::ChannelToString(UnpackOutputs[OutputIndex].SourceChannel)))
											: FText::GetEmpty();
									})
							]
					]
				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SEditableTextBox)
							.Text_Lambda([this, OutputIndex]()
								{
									return UnpackOutputs.IsValidIndex(OutputIndex)
										? FText::FromString(UnpackOutputs[OutputIndex].OutputSuffix)
										: FText::GetEmpty();
								})
							.OnTextCommitted_Lambda([this, OutputIndex](const FText& NewText, ETextCommit::Type)
								{
									if (UnpackOutputs.IsValidIndex(OutputIndex))
									{
										UnpackOutputs[OutputIndex].OutputSuffix = NewText.ToString();
									}
								})
					]
			];
	}

	return Box;
}

void STextureChannelPackerWidget::InvalidatePreview()
{
	// A rebuilt request must not keep a successful preview from a different preset or selection.
	PreviewTexture.Reset();
	PreviewBrush = FSlateBrush();
	PreviewStatusText = FText::GetEmpty();
	if (PreviewImage.IsValid())
	{
		PreviewImage->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Prepass | EInvalidateWidgetReason::Paint);
	}
	if (PreviewScaleBox.IsValid())
	{
		PreviewScaleBox->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Prepass);
	}
}

void STextureChannelPackerWidget::RefreshDynamicPanels()
{
	InvalidatePreview();
	if (PackMappingsPanel.IsValid())
	{
		PackMappingsPanel->SetContent(BuildPackMappings());
	}

	if (UnpackSettingsPanel.IsValid())
	{
		UnpackSettingsPanel->SetContent(BuildUnpackSettings());
	}
}

TSharedRef<SWidget> STextureChannelPackerWidget::BuildTexturePicker(TWeakObjectPtr<UTexture>& TextureSlot)
{
	TWeakObjectPtr<UTexture>* TextureSlotPtr = &TextureSlot;
	return SNew(SObjectPropertyEntryBox)
		.AllowedClass(UTexture::StaticClass())
		.ObjectPath_Lambda([TextureSlotPtr]()
			{
				return TextureSlotPtr->IsValid() ? TextureSlotPtr->Get()->GetPathName() : FString();
			})
		.OnObjectChanged_Lambda([this, TextureSlotPtr](const FAssetData& AssetData)
			{
				InvalidatePreview();
				*TextureSlotPtr = Cast<UTexture>(AssetData.GetAsset());
				AutoGenerateOutputName();
			})
		.AllowClear(true)
		.DisplayThumbnail(true)
		.ThumbnailPool(UThumbnailManager::Get().GetSharedThumbnailPool());
}

TSharedRef<SWidget> STextureChannelPackerWidget::BuildChannelRowTexturePicker(const int32 RowIndex)
{
	return SNew(SObjectPropertyEntryBox)
		.AllowedClass(UTexture::StaticClass())
		.ObjectPath_Lambda([this, RowIndex]()
			{
				if (!ChannelRows.IsValidIndex(RowIndex))
				{
					return FString();
				}

				const TWeakObjectPtr<UTexture>& Texture = ChannelRows[RowIndex].Texture;
				return Texture.IsValid() ? Texture.Get()->GetPathName() : FString();
			})
		.OnObjectChanged_Lambda([this, RowIndex](const FAssetData& AssetData)
			{
				InvalidatePreview();
				if (ChannelRows.IsValidIndex(RowIndex))
				{
					ChannelRows[RowIndex].Texture = Cast<UTexture>(AssetData.GetAsset());
					AutoGenerateOutputName();
				}
			})
		.AllowClear(true)
		.DisplayThumbnail(true)
		.ThumbnailPool(UThumbnailManager::Get().GetSharedThumbnailPool());
}

TSharedRef<SWidget> STextureChannelPackerWidget::BuildChannelCombo(const int32 RowIndex)
{
	return SNew(SComboBox<TSharedPtr<FString>>)
		.OptionsSource(&ChannelOptions)
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
			{
				return SNew(STextBlock).Text(Item.IsValid() ? FText::FromString(*Item) : FText::GetEmpty());
			})
		.OnSelectionChanged_Lambda([this, RowIndex](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
			{
				InvalidatePreview();
				if (NewSelection.IsValid() && ChannelRows.IsValidIndex(RowIndex))
				{
					ChannelRows[RowIndex].SourceChannel = FTextureChannelPackerUIConversions::StringToChannel(*NewSelection);
				}
			})
		[
			SNew(STextBlock).Text_Lambda([this, RowIndex]()
				{
					return ChannelRows.IsValidIndex(RowIndex)
						? FText::FromString(FTextureChannelPackerUIConversions::ChannelToString(ChannelRows[RowIndex].SourceChannel))
						: FText::GetEmpty();
				})
		];
}

TSharedRef<SWidget> STextureChannelPackerWidget::BuildStringCombo(TArray<TSharedPtr<FString>>& Options, TSharedPtr<FString>& CurrentOption)
{
	TSharedPtr<FString>* CurrentOptionPtr = &CurrentOption;
	return SNew(SComboBox<TSharedPtr<FString>>)
		.OptionsSource(&Options)
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
			{
				return SNew(STextBlock).Text(Item.IsValid() ? FText::FromString(*Item) : FText::GetEmpty());
			})
		.OnSelectionChanged_Lambda([CurrentOptionPtr](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
			{
				if (NewSelection.IsValid())
				{
					*CurrentOptionPtr = NewSelection;
				}
			})
		[
			SNew(STextBlock).Text_Lambda([CurrentOptionPtr]() { return CurrentOptionPtr->IsValid() ? FText::FromString(**CurrentOptionPtr) : FText::GetEmpty(); })
		];
	/* clang-format on */
}

void STextureChannelPackerWidget::ApplyPreset(const FTextureChannelPackerPreset& Preset)
{
	Operation = Preset.Operation;
	OutputSettings = Preset.OutputSettings;
	OutputSuffix = Preset.OutputSuffix;

	for (TSharedPtr<FString>& Option : OperationOptions)
	{
		if (Option.IsValid() && *Option == FTextureChannelPackerUIConversions::OperationToString(Operation))
		{
			CurrentOperationOption = Option;
			break;
		}
	}

	for (TSharedPtr<FString>& Option : CompressionOptions)
	{
		if (Option.IsValid() && *Option == FTextureChannelPackerUIConversions::CompressionToString(OutputSettings.CompressionSettings))
		{
			CurrentCompressionOption = Option;
			break;
		}
	}

	for (TSharedPtr<FString>& Option : MipOptions)
	{
		if (Option.IsValid() && *Option == FTextureChannelPackerUIConversions::MipToString(OutputSettings.MipGenSettings))
		{
			CurrentMipOption = Option;
			break;
		}
	}

	for (TSharedPtr<FString>& Option : FilterOptions)
	{
		if (Option.IsValid() && *Option == FTextureChannelPackerUIConversions::FilterToString(OutputSettings.Filter))
		{
			CurrentFilterOption = Option;
			break;
		}
	}

	for (TSharedPtr<FString>& Option : LODGroupOptions)
	{
		if (Option.IsValid() && *Option == FTextureChannelPackerUIConversions::LODGroupToString(OutputSettings.LODGroup))
		{
			CurrentLODGroupOption = Option;
			break;
		}
	}

	if (Operation == ETextureChannelPackerOperation::Unpack)
	{
		UnpackOutputs.Reset();
		for (const FTextureChannelPackerPresetChannel& PresetChannel : Preset.Channels)
		{
			FTextureChannelPackerUnpackOutput Output;
			Output.SourceChannel = PresetChannel.SourceChannel;
			Output.OutputSuffix = PresetChannel.OutputSuffix;
			Output.bEnabled = PresetChannel.bEnabled;
			UnpackOutputs.Add(Output);
		}
	}
	else
	{
		for (FChannelRowState& Row : ChannelRows)
		{
			Row.Texture.Reset();
			Row.SourcePattern.Reset();
			Row.SourceChannel = Row.OutputChannel;
			Row.bUseConstant = Row.OutputChannel == ETextureChannelPackerChannel::Alpha;
			Row.ConstantValue = Row.OutputChannel == ETextureChannelPackerChannel::Alpha ? 255 : 0;
		}

		for (const FTextureChannelPackerPresetChannel& PresetChannel : Preset.Channels)
		{
			const int32 OutputIndex = static_cast<int32>(PresetChannel.OutputChannel);
			if (ChannelRows.IsValidIndex(OutputIndex))
			{
				ChannelRows[OutputIndex].SourceChannel = PresetChannel.SourceChannel;
				ChannelRows[OutputIndex].SourcePattern = PresetChannel.SourcePattern;
				ChannelRows[OutputIndex].bUseConstant = PresetChannel.bUseConstant;
				ChannelRows[OutputIndex].ConstantValue = PresetChannel.ConstantValue;
			}
		}
	}

	if (!bOutputNameManuallyEdited)
	{
		OutputBaseName = TEXT("T_Packed_Texture") + Preset.OutputSuffix;
	}

	RefreshDynamicPanels();
}

void STextureChannelPackerWidget::ApplySelectedContentBrowserTextures()
{
	FContentBrowserModule& ContentBrowserModule =
		FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	TArray<UTexture*> SelectedTextures;
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (UTexture* Texture = Cast<UTexture>(AssetData.GetAsset()))
		{
			SelectedTextures.Add(Texture);
		}
	}

	if (SelectedTextures.Num() == 0)
	{
		ShowNotification(LOCTEXT("NoSelectedTextures", "Select one or more textures in the Content Browser first."),
			false);
		return;
	}

	if (CurrentPresetOption.IsValid())
	{
		if (const FTextureChannelPackerPreset* Preset = UTextureChannelPackerSettings::Get()->FindPreset(*CurrentPresetOption))
		{
			FTextureChannelPackerRequest Request = BuildRequest();
			FTextureChannelPackerPatternService::ApplyPresetPatterns(*Preset, SelectedTextures, Request);
			Operation = Request.Operation;
			OutputSettings = Request.OutputSettings;
			OutputBaseName = Request.OutputBaseName;
			bOutputNameManuallyEdited = false;

			for (FChannelRowState& Row : ChannelRows)
			{
				Row.Texture.Reset();
			}

			for (const FTextureChannelPackerMapping& Mapping : Request.Mappings)
			{
				const int32 OutputIndex = static_cast<int32>(Mapping.OutputChannel);
				if (ChannelRows.IsValidIndex(OutputIndex))
				{
					ChannelRows[OutputIndex].Texture = Mapping.Source.Texture;
					ChannelRows[OutputIndex].SourceChannel = Mapping.Source.Channel;
					ChannelRows[OutputIndex].bUseConstant = Mapping.Source.bUseConstant;
					ChannelRows[OutputIndex].ConstantValue = Mapping.Source.ConstantValue;
					ChannelRows[OutputIndex].SourcePattern = Mapping.SourcePattern;
				}
			}

			UnpackSourceTexture = Request.UnpackSourceTexture;
			UnpackOutputs = Request.UnpackOutputs.Num() > 0 ? Request.UnpackOutputs : UnpackOutputs;
			RefreshDynamicPanels();
			ShowNotification(
				LOCTEXT("SelectedTexturesApplied", "Selected textures were applied to the current preset."), true);
			return;
		}
	}

	if (Operation == ETextureChannelPackerOperation::Unpack)
	{
		UnpackSourceTexture = SelectedTextures[0];
	}
	else
	{
		for (int32 Index = 0; Index < ChannelRows.Num() && Index < SelectedTextures.Num(); ++Index)
		{
			ChannelRows[Index].Texture = SelectedTextures[Index];
			ChannelRows[Index].bUseConstant = false;
		}
	}

	AutoGenerateOutputName();
	RefreshDynamicPanels();
}

FReply STextureChannelPackerWidget::OnGenerateClicked()
{
	const FTextureChannelPackerResult Result = FTextureChannelPackerTextureService::Execute(BuildRequest());
	ShowNotification(Result.Message, Result.bSuccess);
	return FReply::Handled();
}

FReply STextureChannelPackerWidget::OnPreviewClicked()
{
	RefreshPreview();
	return FReply::Handled();
}

FReply STextureChannelPackerWidget::OnSavePresetClicked()
{
	const FTextureChannelPackerPreset Preset = FTextureChannelPackerPatternService::CreatePreset(
		BuildRequest(), OutputBaseName.IsEmpty() ? TEXT("Custom Texture Packing") : OutputBaseName, OutputSuffix);

	if (!UTextureChannelPackerSettings::GetMutable()->SavePreset(Preset))
	{
		ShowNotification(LOCTEXT("PresetSaveFailed", "Could not save preset. Check that Config/DefaultEditor.ini is writable."), false);
		return FReply::Handled();
	}
	RefreshPresetOptions();
	// Keep subsequent Content Browser matching on the saved preset rather than the first built-in entry.
	for (const TSharedPtr<FString>& Option : PresetOptions)
	{
		if (Option.IsValid() && *Option == Preset.Name)
		{
			CurrentPresetOption = Option;
			break;
		}
	}
	ShowNotification(LOCTEXT("PresetSaved", "Preset saved."), true);
	return FReply::Handled();
}

FTextureChannelPackerRequest STextureChannelPackerWidget::BuildRequest() const
{
	FTextureChannelPackerRequest Request;
	Request.Operation = Operation;
	Request.OutputPackagePath = OutputPackagePath;
	Request.OutputBaseName = OutputBaseName;
	Request.OutputSettings = OutputSettings;

	if (Operation == ETextureChannelPackerOperation::Unpack)
	{
		Request.UnpackSourceTexture = UnpackSourceTexture;
		Request.UnpackOutputs = UnpackOutputs;
		return Request;
	}

	TWeakObjectPtr<UTexture> RepackSource;
	if (Operation == ETextureChannelPackerOperation::Repack)
	{
		for (const FChannelRowState& Row : ChannelRows)
		{
			if (Row.Texture.IsValid())
			{
				RepackSource = Row.Texture;
				break;
			}
		}
	}

	for (const FChannelRowState& Row : ChannelRows)
	{
		FTextureChannelPackerMapping Mapping;
		Mapping.OutputChannel = Row.OutputChannel;
		Mapping.Source.Channel = Row.SourceChannel;
		Mapping.Source.Texture = Operation == ETextureChannelPackerOperation::Repack && RepackSource.IsValid() ? RepackSource : Row.Texture;
		Mapping.Source.bUseConstant = Row.bUseConstant;
		Mapping.Source.ConstantValue = static_cast<uint8>(FMath::Clamp(Row.ConstantValue, 0, 255));
		Mapping.SourcePattern = Row.SourcePattern;
		Request.Mappings.Add(Mapping);
	}

	return Request;
}

void STextureChannelPackerWidget::ShowNotification(const FText& Message, const bool bSuccess) const
{
	FNotificationInfo Info(Message.IsEmpty() ? LOCTEXT("UnknownResult", "Texture channel operation finished.")
											 : Message);
	Info.ExpireDuration = 4.0f;
	Info.Image = FAppStyle::GetBrush(bSuccess ? "Icons.SuccessWithColor" : "Icons.ErrorWithColor");

	TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
	if (NotificationItem.IsValid())
	{
		NotificationItem->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		NotificationItem->ExpireAndFadeout();
	}
}

/** @brief Regenerates preview data and invalidates image layout after changing the brush in place. */
void STextureChannelPackerWidget::RefreshPreview()
{
	const FTextureChannelPackerPreviewResult Result =
		FTextureChannelPackerTextureService::BuildPreview(BuildRequest(), PreviewMode);
	PreviewStatusText = Result.Message;

	PreviewTexture.Reset();
	PreviewBrush = FSlateBrush();

	if (Result.bSuccess && Result.PreviewTexture)
	{
		PreviewTexture = TStrongObjectPtr<UTexture2D>(Result.PreviewTexture);
		PreviewBrush.DrawAs = ESlateBrushDrawType::Image;
		PreviewBrush.SetResourceObject(PreviewTexture.Get());
		PreviewBrush.ImageSize =
			FVector2D(static_cast<float>(Result.PreviewSize.X), static_cast<float>(Result.PreviewSize.Y));
	}
	else
	{
		ShowNotification(Result.Message, false);
	}
	if (PreviewImage.IsValid())
	{
		// The brush pointer stays unchanged, so Slate cannot detect its new size or resource automatically.
		PreviewImage->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Prepass | EInvalidateWidgetReason::Paint);
	}
	if (PreviewScaleBox.IsValid())
	{
		// SScaleBox owns a cached normalization prepass; invalidating only its image leaves a zero scale cached.
		PreviewScaleBox->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Prepass);
	}
}

void STextureChannelPackerWidget::AutoGenerateOutputName()
{
	if (bOutputNameManuallyEdited)
	{
		return;
	}

	TArray<UTexture*> Textures;
	if (Operation == ETextureChannelPackerOperation::Unpack)
	{
		if (UnpackSourceTexture.IsValid())
		{
			Textures.Add(UnpackSourceTexture.Get());
		}
	}
	else
	{
		for (const FChannelRowState& Row : ChannelRows)
		{
			if (Row.Texture.IsValid())
			{
				Textures.Add(Row.Texture.Get());
			}
		}
	}

	OutputBaseName = FTextureChannelPackerPatternService::BuildBaseNameFromTextures(Textures, OutputBaseName);
}

EVisibility STextureChannelPackerWidget::GetPackVisibility() const
{
	return Operation == ETextureChannelPackerOperation::Unpack ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility STextureChannelPackerWidget::GetUnpackVisibility() const
{
	return Operation == ETextureChannelPackerOperation::Unpack ? EVisibility::Visible : EVisibility::Collapsed;
}

FText STextureChannelPackerWidget::GetOperationText() const
{
	return CurrentOperationOption.IsValid() ? FText::FromString(*CurrentOperationOption) : FText::GetEmpty();
}

FText STextureChannelPackerWidget::GetPresetText() const
{
	return CurrentPresetOption.IsValid() ? FText::FromString(*CurrentPresetOption) : LOCTEXT("NoPreset", "No Preset");
}

FText STextureChannelPackerWidget::GetPreviewStatusText() const
{
	return PreviewStatusText.IsEmpty()
		? LOCTEXT("PreviewStatusInitial", "Press Preview to render the current channel setup.")
		: PreviewStatusText;
}

EVisibility STextureChannelPackerWidget::GetPreviewImageVisibility() const
{
	return PreviewTexture.Get() != nullptr ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility STextureChannelPackerWidget::GetPreviewPlaceholderVisibility() const
{
	return PreviewTexture.Get() != nullptr ? EVisibility::Collapsed : EVisibility::Visible;
}

FSlateColor STextureChannelPackerWidget::GetPreviewModeButtonColor(const ETextureChannelPackerPreviewMode Mode) const
{
	return PreviewMode == Mode ? FSlateColor(FLinearColor(0.18f, 0.32f, 0.52f, 1.0f)) : FSlateColor(FLinearColor::White);
}

#undef LOCTEXT_NAMESPACE
