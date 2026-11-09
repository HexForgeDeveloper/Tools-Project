#pragma once

#include "Commandlets/Commandlet.h"
#include "AuditRedirectorsCommandlet.generated.h"

/** @brief Reports redirectors under /Game and the saved packages that reference them. */
UCLASS(meta = (DisplayName = "Audit Redirectors"))
class TOOLSPROJECTEDITOR_API UAuditRedirectorsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	/** @brief Sets the commandlet's editor mode and help text. */
	UAuditRedirectorsCommandlet();

	/**
	 * @brief Scans saved asset metadata and writes a read-only report to the Unreal log.
	 * @param Params Command-line parameters. The audit uses /Game as its fixed scope.
	 * @return Zero when the audit completes, including when redirectors are found.
	 */
	virtual int32 Main(const FString& Params) override;
};
