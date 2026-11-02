# CppTemplateGenerator — Roadmap

## Выполнено

- [x] Убрать `namespace TemplateGeneratorSettings`: очистку списка реализовать приватным методом `UCppTemplateGeneratorSettings::SanitizeTemplateClassesInternal(bool bPreserveNullEntries)`, работающим непосредственно с `TemplateClasses`. Сохранить удаление неподдерживаемых классов и дубликатов, пустые элементы при редактировании и очистку при инициализации.
