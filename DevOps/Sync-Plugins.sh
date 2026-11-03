#!/usr/bin/env bash
set -euo pipefail

source_root=$(realpath "${1:?Usage: Sync-Plugins.sh SOURCE_PROJECT DESTINATION_REPOSITORY}")
destination_root=$(realpath "${2:?Destination repository is required}")
[[ "$source_root" != "$destination_root" ]] || { echo 'Source and destination must differ.' >&2; exit 1; }
[[ -d "$destination_root/.git" || -f "$destination_root/.git" ]] || { echo 'Destination must be a Git checkout.' >&2; exit 1; }

plugins=(ContentBrowserToolkit CppTemplateGenerator DataAssetManager OutlinerToolkit PerformanceInspector TextureChannelPacker UNotepad ValidatorX WireBundle)

# Validate every source before modifying any destination.
for plugin in "${plugins[@]}"; do
  directory="$source_root/Plugins/$plugin"
  if [[ "$plugin" == PerformanceInspector ]]; then directory="$directory/PerformanceInspector"; fi
  [[ -f "$directory/$plugin.uplugin" ]] || { echo "Missing descriptor: $directory/$plugin.uplugin" >&2; exit 1; }
done

for plugin in "${plugins[@]}"; do
  directory="$source_root/Plugins/$plugin"
  if [[ "$plugin" == PerformanceInspector ]]; then directory="$directory/PerformanceInspector"; fi
  echo "Synchronizing $plugin"
  # Deletions propagate only inside these explicitly managed plugin directories.
  # Excluded local files are preserved and never copied from the source.
  rsync -r --checksum --delete --itemize-changes \
    --exclude='.git/' --exclude='.agents/' --exclude='.codex/' --exclude='.claude/' \
    --exclude='Binaries/' --exclude='Intermediate/' --exclude='Saved/' --exclude='DerivedDataCache/' \
    --exclude='AGENTS.md' --exclude='CLAUDE.md' --exclude='GEMINI.md' \
    --exclude='ROADMAP.md' --exclude='RELEASE_PLAN.md' --exclude='REALEASE_PLAN.md' \
    --exclude='COMMIT_MESSAGE.md' \
    "$directory/" "$destination_root/$plugin/"
done
