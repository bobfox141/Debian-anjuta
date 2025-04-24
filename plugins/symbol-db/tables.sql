
-- FIXME: FOREIGN keys are now supported. schema should enforces them.
-- Right now the support here is disabled.

DROP TABLE IF EXISTS workspace;
CREATE TABLE workspace (workspace_id integer PRIMARY KEY AUTOINCREMENT,
                        workspace_name text not null unique,
                        analyse_time date
                        );

DROP TABLE IF EXISTS project;
CREATE TABLE project (project_id integer PRIMARY KEY AUTOINCREMENT,
                      project_name text not null,
                      project_version text not null default '1.0',
                      wrkspace_id integer REFERENCES workspace (workspace_id),
                      analyse_time date,
                      unique (project_name, project_version)
                      );

DROP TABLE IF EXISTS file;
CREATE TABLE file (file_id integer PRIMARY KEY AUTOINCREMENT,
                   file_path text not null unique,
                   prj_id integer REFERENCES project (projec_id),
                   lang_id integer REFERENCES language (language_id),
                   analyse_time date
                   );

DROP TABLE IF EXISTS language;
CREATE TABLE language (language_id integer PRIMARY KEY AUTOINCREMENT,
                       language_name text not null unique);

DROP TABLE IF EXISTS symbol;
CREATE TABLE symbol (symbol_id integer PRIMARY KEY AUTOINCREMENT,
                     file_defined_id integer not null REFERENCES file (file_id),
                     name text not null,
                     file_position integer,
                     is_file_scope integer,
                     signature text,
                     returntype text,
                     scope_definition_id integer not null,
                     scope_id integer not null,
                     type_type text not null,
					 type_name text not null,
                     kind_id integer REFERENCES sym_kind (sym_kind_id),
                     access_kind_id integer REFERENCES sym_access (sym_access_id),
                     implementation_kind_id integer REFERENCES sym_implementation (sym_impl_id),
                     update_flag integer default 0,
                     unique (name, file_defined_id, file_position, update_flag)
                     );

DROP TABLE IF EXISTS sym_kind;
CREATE TABLE sym_kind (sym_kind_id integer PRIMARY KEY AUTOINCREMENT,
                       kind_name text not null unique,
                       is_container integer default 0
                       );

DROP TABLE IF EXISTS sym_access;
CREATE TABLE sym_access (access_kind_id integer PRIMARY KEY AUTOINCREMENT,
                         access_name text not null unique
                         );

DROP TABLE IF EXISTS sym_implementation;
CREATE TABLE sym_implementation (sym_impl_id integer PRIMARY KEY AUTOINCREMENT,
                                 implementation_name text not null unique
                                 );

DROP TABLE IF EXISTS heritage;
CREATE TABLE heritage (symbol_id_base integer REFERENCES symbol (symbol_id),
                       symbol_id_derived integer REFERENCES symbol (symbol_id),
                       PRIMARY KEY (symbol_id_base, symbol_id_derived)
                       );

DROP TABLE IF EXISTS scope;
CREATE TABLE scope (scope_id integer PRIMARY KEY AUTOINCREMENT,
                    scope_name text not null,
                    unique (scope_name)
                    );

DROP TABLE IF EXISTS version;
CREATE TABLE version (sdb_version numeric PRIMARY KEY);


DROP TABLE IF EXISTS __tmp_removed;
CREATE TABLE __tmp_removed (tmp_removed_id integer PRIMARY KEY AUTOINCREMENT,
                            symbol_removed_id integer not null
                            );


DROP INDEX IF EXISTS symbol_idx_1;
CREATE INDEX symbol_idx_1 ON symbol (name, file_defined_id, type_type, type_name);

-- removing this index isn't worth because the performance gain is invisible.
DROP INDEX IF EXISTS symbol_idx_2;
CREATE INDEX symbol_idx_2 ON symbol (scope_id);

-- removing this index isn't worth because the performance gain is invisible.
DROP INDEX IF EXISTS symbol_idx_3;
CREATE INDEX symbol_idx_3 ON symbol (type_type, type_name);


DROP TRIGGER IF EXISTS delete_file_trg;
CREATE TRIGGER delete_file_trg BEFORE DELETE ON file
FOR EACH ROW
BEGIN
	DELETE FROM symbol WHERE file_defined_id = old.file_id;
END;

DROP TRIGGER IF EXISTS delete_symbol_trg;
CREATE TRIGGER delete_symbol_trg BEFORE DELETE ON symbol
FOR EACH ROW
BEGIN
    DELETE FROM scope WHERE scope.scope_id=old.scope_definition_id;
    UPDATE symbol SET scope_id='-1' WHERE symbol.scope_id=old.scope_definition_id AND symbol.scope_id > 0;
    INSERT INTO __tmp_removed (symbol_removed_id) VALUES (old.symbol_id);
END;

PRAGMA page_size = 32768;
PRAGMA cache_size = 12288;
PRAGMA synchronous = OFF;
PRAGMA temp_store = MEMORY;
PRAGMA case_sensitive_like = 1;
PRAGMA journal_mode = OFF;
PRAGMA read_uncommitted = 1;
PRAGMA foreign_keys = OFF;
PRAGMA auto_vacuum = 0;

