[+ autogen5 template +]
[+INCLUDE (string-append "indent.tpl") \+]
[+INCLUDE (string-append "licenses/" (get "License") ".tpl") \+]
/* [+INVOKE EMACS-MODELINE MODE="C" \+] */
[+INVOKE START-INDENT\+]
/*
 * [+HeaderFile+][+IF (=(get "Headings") "1")+]
 * Copyright (C) [+(shell "date +%Y")+] [+AuthorName+] <[+AuthorEmail+]>[+ENDIF+]
 *
[+INVOKE LICENSE-DESCRIPTION PFX=" * " PROGRAM=(get "ProjectName") OWNER=(get "AuthorName") \+]
 */

#ifndef _[+ (string-upcase(string->c-name!(get "HeaderFile"))) +]_
#define _[+ (string-upcase(string->c-name!(get "HeaderFile"))) +]_

#include <glib-object.h>

G_BEGIN_DECLS

#define [+TypePrefix+]_TYPE_[+TypeSuffix+]             ([+FuncPrefix+]_get_type ())
#define [+TypePrefix+]_[+TypeSuffix+](obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), [+TypePrefix+]_TYPE_[+TypeSuffix+], [+ClassName+]))
#define [+TypePrefix+]_[+TypeSuffix+]_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), [+TypePrefix+]_TYPE_[+TypeSuffix+], [+ClassName+]Class))
#define [+TypePrefix+]_IS_[+TypeSuffix+](obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), [+TypePrefix+]_TYPE_[+TypeSuffix+]))
#define [+TypePrefix+]_IS_[+TypeSuffix+]_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), [+TypePrefix+]_TYPE_[+TypeSuffix+]))
#define [+TypePrefix+]_[+TypeSuffix+]_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), [+TypePrefix+]_TYPE_[+TypeSuffix+], [+ClassName+]Class))

typedef struct _[+ClassName+]Class [+ClassName+]Class;
typedef struct _[+ClassName+] [+ClassName+];
typedef struct _[+ClassName+]Private [+ClassName+]Private;


struct _[+ClassName+]Class
{
	[+BaseClass+]Class parent_class;[+
IF (not (=(get "Signals[0].Name") ""))+]

	/* Signals */
[+
	FOR Signals "\n" +]	[+
		Type+](* [+
		(string->c-name!(get "Name"))+]) [+
		Arguments+];[+
	ENDFOR+][+
ENDIF+]
};

struct _[+ClassName+]
{
	[+BaseClass+] parent_instance;[+
IF (not (=(get "PublicVariableCount") "0"))+]
[+
	FOR Members+][+
		IF (=(get "Scope") "public") +][+
			IF (=(get "Arguments") "")+]
	[+
				Type+] [+
				Name+];[+
			ENDIF+][+
		ENDIF+][+
	ENDFOR+][+
ENDIF+]

	[+ClassName+]Private *priv;
};

GType [+FuncPrefix+]_get_type (void) G_GNUC_CONST;[+
IF (not (=(get "PublicFunctionCount") "0"))+][+
	FOR Members+][+
		IF (=(get "Scope") "public") +][+
			IF (not(=(get "Arguments") ""))+]
[+
				Type+] [+
				FuncPrefix+]_[+
				Name+] [+
				Arguments+];[+
			ENDIF+][+
		ENDIF+][+
	ENDFOR+][+
ENDIF+]

G_END_DECLS

#endif /* _[+ (string-upcase(string->c-name!(get "HeaderFile"))) +]_ */
[+INVOKE END-INDENT\+]
