
/*  A Bison parser, made from objc-parse.y
 by  GNU Bison version 1.25
  */

#define YYBISON 1  /* Identify Bison output.  */

#define	IDENTIFIER	258
#define	TYPENAME	259
#define	SCSPEC	260
#define	STATIC	261
#define	TYPESPEC	262
#define	TYPE_QUAL	263
#define	CONSTANT	264
#define	STRING	265
#define	ELLIPSIS	266
#define	SIZEOF	267
#define	ENUM	268
#define	STRUCT	269
#define	UNION	270
#define	IF	271
#define	ELSE	272
#define	WHILE	273
#define	DO	274
#define	FOR	275
#define	SWITCH	276
#define	CASE	277
#define	DEFAULT	278
#define	BREAK	279
#define	CONTINUE	280
#define	RETURN	281
#define	GOTO	282
#define	ASM_KEYWORD	283
#define	TYPEOF	284
#define	ALIGNOF	285
#define	ATTRIBUTE	286
#define	EXTENSION	287
#define	LABEL	288
#define	REALPART	289
#define	IMAGPART	290
#define	VA_ARG	291
#define	CHOOSE_EXPR	292
#define	TYPES_COMPATIBLE_P	293
#define	PTR_VALUE	294
#define	PTR_BASE	295
#define	PTR_EXTENT	296
#define	STRING_FUNC_NAME	297
#define	VAR_FUNC_NAME	298
#define	ASSIGN	299
#define	OROR	300
#define	ANDAND	301
#define	EQCOMPARE	302
#define	ARITHCOMPARE	303
#define	LSHIFT	304
#define	RSHIFT	305
#define	UNARY	306
#define	PLUSPLUS	307
#define	MINUSMINUS	308
#define	HYPERUNARY	309
#define	POINTSAT	310
#define	INTERFACE	311
#define	IMPLEMENTATION	312
#define	END	313
#define	SELECTOR	314
#define	DEFS	315
#define	ENCODE	316
#define	CLASSNAME	317
#define	PUBLIC	318
#define	PRIVATE	319
#define	PROTECTED	320
#define	PROTOCOL	321
#define	OBJECTNAME	322
#define	CLASS	323
#define	ALIAS	324

#line 33 "objc-parse.y"

#include "config.h"
#include "system.h"
#include "tree.h"
#include "input.h"
#include "cpplib.h"
#include "intl.h"
#include "timevar.h"
#include "c-pragma.h"		/* For YYDEBUG definition, and parse_in.  */
#include "c-tree.h"
#include "flags.h"
#include "output.h"
#include "toplev.h"
#include "ggc.h"

#ifdef MULTIBYTE_CHARS
#include <locale.h>
#endif

#include "objc-act.h"

/* Like YYERROR but do call yyerror.  */
#define YYERROR1 { yyerror ("syntax error"); YYERROR; }

/* Like the default stack expander, except (1) use realloc when possible,
   (2) impose no hard maxiumum on stack size, (3) REALLY do not use alloca.

   Irritatingly, YYSTYPE is defined after this %{ %} block, so we cannot
   give malloced_yyvs its proper type.  This is ok since all we need from
   it is to be able to free it.  */

static short *malloced_yyss;
static void *malloced_yyvs;

#define yyoverflow(MSG, SS, SSSIZE, VS, VSSIZE, YYSSZ)			\
do {									\
  size_t newsize;							\
  short *newss;								\
  YYSTYPE *newvs;							\
  newsize = *(YYSSZ) *= 2;						\
  if (malloced_yyss)							\
    {									\
      newss = (short *)							\
	really_call_realloc (*(SS), newsize * sizeof (short));		\
      newvs = (YYSTYPE *)						\
	really_call_realloc (*(VS), newsize * sizeof (YYSTYPE));	\
    }									\
  else									\
    {									\
      newss = (short *) really_call_malloc (newsize * sizeof (short));	\
      newvs = (YYSTYPE *) really_call_malloc (newsize * sizeof (YYSTYPE)); \
      if (newss)							\
        memcpy (newss, *(SS), (SSSIZE));				\
      if (newvs)							\
        memcpy (newvs, *(VS), (VSSIZE));				\
    }									\
  if (!newss || !newvs)							\
    {									\
      yyerror (MSG);							\
      return 2;								\
    }									\
  *(SS) = newss;							\
  *(VS) = newvs;							\
  malloced_yyss = newss;						\
  malloced_yyvs = (void *) newvs;					\
} while (0)

#line 103 "objc-parse.y"
typedef union {long itype; tree ttype; enum tree_code code;
	const char *filename; int lineno; } YYSTYPE;
#line 248 "objc-parse.y"

/* Number of statements (loosely speaking) and compound statements
   seen so far.  */
static int stmt_count;
static int compstmt_count;

/* Input file and line number of the end of the body of last simple_if;
   used by the stmt-rule immediately after simple_if returns.  */
static const char *if_stmt_file;
static int if_stmt_line;

/* List of types and structure classes of the current declaration.  */
static GTY(()) tree current_declspecs;
static GTY(()) tree prefix_attributes;

/* List of all the attributes applying to the identifier currently being
   declared; includes prefix_attributes and possibly some more attributes
   just after a comma.  */
static GTY(()) tree all_prefix_attributes;

/* Stack of saved values of current_declspecs, prefix_attributes and
   all_prefix_attributes.  */
static GTY(()) tree declspec_stack;

/* PUSH_DECLSPEC_STACK is called from setspecs; POP_DECLSPEC_STACK
   should be called from the productions making use of setspecs.  */
#define PUSH_DECLSPEC_STACK						 \
  do {									 \
    declspec_stack = tree_cons (build_tree_list (prefix_attributes,	 \
						 all_prefix_attributes), \
				current_declspecs,			 \
				declspec_stack);			 \
  } while (0)

#define POP_DECLSPEC_STACK						\
  do {									\
    current_declspecs = TREE_VALUE (declspec_stack);			\
    prefix_attributes = TREE_PURPOSE (TREE_PURPOSE (declspec_stack));	\
    all_prefix_attributes = TREE_VALUE (TREE_PURPOSE (declspec_stack));	\
    declspec_stack = TREE_CHAIN (declspec_stack);			\
  } while (0)

/* For __extension__, save/restore the warning flags which are
   controlled by __extension__.  */
#define SAVE_EXT_FLAGS()			\
	size_int (pedantic			\
		  | (warn_pointer_arith << 1)	\
		  | (warn_traditional << 2)	\
		  | (flag_iso << 3))

#define RESTORE_EXT_FLAGS(tval)			\
  do {						\
    int val = tree_low_cst (tval, 0);		\
    pedantic = val & 1;				\
    warn_pointer_arith = (val >> 1) & 1;	\
    warn_traditional = (val >> 2) & 1;		\
    flag_iso = (val >> 3) & 1;			\
  } while (0)

/* Objective-C specific parser/lexer information */

static enum tree_code objc_inherit_code;
static int objc_pq_context = 0, objc_public_flag = 0;

/* The following flag is needed to contextualize ObjC lexical analysis.
   In some cases (e.g., 'int NSObject;'), it is undesirable to bind
   an identifier to an ObjC class, even if a class with that name
   exists.  */
static int objc_need_raw_identifier;
#define OBJC_NEED_RAW_IDENTIFIER(VAL)	objc_need_raw_identifier = VAL


static bool parsing_iso_function_signature;

/* Tell yyparse how to print a token's value, if yydebug is set.  */

#define YYPRINT(FILE,YYCHAR,YYLVAL) yyprint(FILE,YYCHAR,YYLVAL)

static void yyprint	  PARAMS ((FILE *, int, YYSTYPE));
static void yyerror	  PARAMS ((const char *));
static int yylexname	  PARAMS ((void));
static int yylexstring	  PARAMS ((void));
static inline int _yylex  PARAMS ((void));
static int  yylex	  PARAMS ((void));
static void init_reswords PARAMS ((void));

  /* Initialisation routine for this file.  */
void
c_parse_init ()
{
  init_reswords ();
}

#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		1165
#define	YYFLAG		-32768
#define	YYNTBASE	93

#define YYTRANSLATE(x) ((unsigned)(x) <= 324 ? yytranslate[x] : 358)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    88,     2,     2,     2,    61,    52,     2,    68,
    84,    59,    57,    89,    58,    67,    60,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,    47,    85,     2,
    45,     2,    46,    92,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
    69,     2,    91,    51,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    90,    50,    86,    87,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     1,     2,     3,     4,     5,
     6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
    16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
    26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
    36,    37,    38,    39,    40,    41,    42,    43,    44,    48,
    49,    53,    54,    55,    56,    62,    63,    64,    65,    66,
    70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
    80,    81,    82,    83
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     1,     3,     4,     7,     8,    12,    14,    16,    18,
    20,    26,    29,    33,    38,    43,    46,    49,    52,    54,
    55,    56,    66,    71,    72,    73,    83,    88,    89,    90,
    99,   103,   105,   107,   109,   111,   113,   115,   117,   119,
   121,   123,   125,   127,   128,   130,   132,   136,   138,   141,
   144,   147,   150,   153,   158,   161,   166,   169,   172,   174,
   176,   178,   180,   185,   187,   191,   195,   199,   203,   207,
   211,   215,   219,   223,   227,   231,   235,   236,   241,   242,
   247,   248,   249,   257,   258,   264,   268,   272,   274,   276,
   278,   280,   281,   289,   293,   297,   301,   305,   310,   317,
   326,   333,   338,   342,   346,   349,   352,   354,   356,   358,
   360,   362,   365,   369,   371,   372,   374,   378,   380,   382,
   385,   388,   393,   398,   401,   404,   408,   409,   411,   416,
   421,   425,   429,   432,   435,   437,   440,   443,   446,   449,
   452,   454,   457,   459,   462,   465,   468,   471,   474,   477,
   479,   482,   485,   488,   491,   494,   497,   500,   503,   506,
   509,   512,   515,   518,   521,   524,   527,   529,   532,   535,
   538,   541,   544,   547,   550,   553,   556,   559,   562,   565,
   568,   571,   574,   577,   580,   583,   586,   589,   592,   595,
   598,   601,   604,   607,   610,   613,   616,   619,   622,   625,
   628,   631,   634,   637,   640,   643,   646,   649,   652,   655,
   658,   661,   663,   665,   667,   669,   671,   673,   675,   677,
   679,   681,   683,   685,   687,   689,   691,   693,   695,   697,
   699,   701,   703,   705,   707,   709,   711,   713,   715,   717,
   719,   721,   723,   725,   727,   729,   731,   733,   735,   737,
   739,   741,   743,   745,   747,   749,   751,   753,   755,   757,
   759,   761,   763,   765,   767,   769,   771,   773,   774,   776,
   778,   780,   782,   784,   786,   788,   790,   793,   796,   798,
   803,   808,   810,   815,   817,   822,   823,   828,   829,   836,
   840,   841,   848,   852,   853,   855,   857,   860,   867,   869,
   873,   874,   876,   881,   888,   893,   895,   897,   899,   901,
   903,   905,   907,   908,   913,   915,   916,   919,   921,   925,
   929,   932,   933,   938,   940,   941,   946,   948,   950,   952,
   955,   958,   964,   968,   969,   970,   978,   979,   980,   988,
   990,   992,   997,  1001,  1004,  1008,  1010,  1012,  1014,  1016,
  1020,  1023,  1025,  1027,  1031,  1034,  1038,  1042,  1047,  1051,
  1056,  1060,  1063,  1065,  1067,  1070,  1072,  1075,  1077,  1080,
  1081,  1089,  1095,  1096,  1104,  1110,  1111,  1120,  1121,  1129,
  1132,  1135,  1138,  1139,  1141,  1142,  1144,  1146,  1149,  1150,
  1154,  1157,  1162,  1166,  1171,  1175,  1177,  1179,  1182,  1184,
  1189,  1191,  1196,  1201,  1208,  1214,  1219,  1226,  1232,  1234,
  1238,  1240,  1242,  1246,  1247,  1251,  1252,  1254,  1255,  1257,
  1260,  1262,  1264,  1266,  1270,  1273,  1277,  1282,  1286,  1289,
  1292,  1294,  1299,  1303,  1308,  1314,  1320,  1322,  1324,  1326,
  1328,  1330,  1333,  1336,  1339,  1342,  1344,  1347,  1350,  1353,
  1355,  1358,  1361,  1364,  1367,  1369,  1372,  1374,  1376,  1378,
  1380,  1383,  1384,  1385,  1386,  1387,  1388,  1390,  1392,  1395,
  1399,  1401,  1404,  1406,  1408,  1414,  1416,  1418,  1421,  1424,
  1427,  1430,  1431,  1437,  1438,  1443,  1444,  1445,  1447,  1450,
  1454,  1458,  1462,  1463,  1468,  1470,  1474,  1475,  1476,  1484,
  1490,  1493,  1494,  1495,  1496,  1497,  1510,  1511,  1518,  1521,
  1523,  1525,  1528,  1532,  1535,  1538,  1541,  1545,  1552,  1561,
  1572,  1585,  1589,  1594,  1596,  1600,  1606,  1609,  1615,  1616,
  1618,  1619,  1621,  1622,  1624,  1626,  1630,  1635,  1643,  1645,
  1649,  1650,  1654,  1657,  1658,  1659,  1666,  1669,  1670,  1672,
  1674,  1678,  1680,  1684,  1689,  1694,  1698,  1703,  1707,  1712,
  1717,  1721,  1726,  1730,  1732,  1733,  1737,  1739,  1742,  1744,
  1748,  1750,  1754,  1756,  1758,  1760,  1762,  1764,  1766,  1768,
  1770,  1774,  1778,  1783,  1784,  1785,  1796,  1797,  1804,  1805,
  1806,  1819,  1820,  1829,  1830,  1837,  1840,  1841,  1850,  1855,
  1856,  1866,  1872,  1873,  1880,  1884,  1885,  1887,  1891,  1895,
  1897,  1899,  1901,  1903,  1904,  1908,  1911,  1915,  1919,  1921,
  1922,  1924,  1929,  1931,  1935,  1938,  1940,  1942,  1943,  1944,
  1945,  1953,  1954,  1955,  1958,  1960,  1962,  1965,  1966,  1970,
  1972,  1974,  1975,  1976,  1982,  1987,  1989,  1995,  1998,  1999,
  2002,  2003,  2005,  2007,  2009,  2012,  2015,  2020,  2023,  2026,
  2028,  2032,  2035,  2038,  2040,  2041,  2044,  2045,  2049,  2051,
  2053,  2056,  2058,  2060,  2062,  2064,  2066,  2068,  2070,  2072,
  2074,  2076,  2078,  2080,  2082,  2084,  2086,  2088,  2090,  2092,
  2094,  2096,  2098,  2100,  2102,  2104,  2106,  2108,  2115,  2119,
  2125,  2128,  2130,  2132,  2134,  2137,  2139,  2143,  2146,  2148,
  2150,  2155,  2157,  2159,  2161,  2164,  2167,  2169,  2174,  2179
};

static const short yyrhs[] = {    -1,
    94,     0,     0,    95,    97,     0,     0,    94,    96,    97,
     0,    98,     0,   100,     0,    99,     0,   295,     0,    28,
    68,   109,    84,    85,     0,   294,    97,     0,   132,   166,
    85,     0,   152,   132,   166,    85,     0,   151,   132,   165,
    85,     0,   158,    85,     0,     1,    85,     0,     1,    86,
     0,    85,     0,     0,     0,   151,   132,   195,   101,   126,
   102,   255,   256,   244,     0,   151,   132,   195,     1,     0,
     0,     0,   152,   132,   200,   103,   126,   104,   255,   256,
   244,     0,   152,   132,   200,     1,     0,     0,     0,   132,
   200,   105,   126,   106,   255,   256,   244,     0,   132,   200,
     1,     0,     3,     0,     4,     0,    81,     0,    76,     0,
    52,     0,    58,     0,    57,     0,    63,     0,    64,     0,
    87,     0,    88,     0,   111,     0,     0,   111,     0,   117,
     0,   111,    89,   117,     0,   123,     0,    59,   116,     0,
   294,   116,     0,   108,   116,     0,    49,   107,     0,   113,
   112,     0,   113,    68,   221,    84,     0,   114,   112,     0,
   114,    68,   221,    84,     0,    34,   116,     0,    35,   116,
     0,    12,     0,    30,     0,    29,     0,   112,     0,    68,
   221,    84,   116,     0,   116,     0,   117,    57,   117,     0,
   117,    58,   117,     0,   117,    59,   117,     0,   117,    60,
   117,     0,   117,    61,   117,     0,   117,    55,   117,     0,
   117,    56,   117,     0,   117,    54,   117,     0,   117,    53,
   117,     0,   117,    52,   117,     0,   117,    50,   117,     0,
   117,    51,   117,     0,     0,   117,    49,   118,   117,     0,
     0,   117,    48,   119,   117,     0,     0,     0,   117,    46,
   120,   109,    47,   121,   117,     0,     0,   117,    46,   122,
    47,   117,     0,   117,    45,   117,     0,   117,    44,   117,
     0,     3,     0,     9,     0,    10,     0,    43,     0,     0,
    68,   221,    84,    90,   124,   181,    86,     0,    68,   109,
    84,     0,    68,     1,    84,     0,   248,   246,    84,     0,
   248,     1,    84,     0,   123,    68,   110,    84,     0,    36,
    68,   117,    89,   221,    84,     0,    37,    68,   117,    89,
   117,    89,   117,    84,     0,    38,    68,   221,    89,   221,
    84,     0,   123,    69,   109,    91,     0,   123,    67,   107,
     0,   123,    66,   107,     0,   123,    63,     0,   123,    64,
     0,   351,     0,   355,     0,   356,     0,   357,     0,   125,
     0,    92,    10,     0,   125,    92,    10,     0,   127,     0,
     0,   129,     0,   255,   256,   130,     0,   128,     0,   236,
     0,   129,   128,     0,   128,   236,     0,   153,   132,   165,
    85,     0,   154,   132,   166,    85,     0,   153,    85,     0,
   154,    85,     0,   255,   256,   134,     0,     0,   172,     0,
   151,   132,   165,    85,     0,   152,   132,   166,    85,     0,
   151,   132,   189,     0,   152,   132,   192,     0,   158,    85,
     0,   294,   134,     0,     8,     0,   135,     8,     0,   136,
     8,     0,   135,   173,     0,   137,     8,     0,   138,     8,
     0,   173,     0,   137,   173,     0,   160,     0,   139,     8,
     0,   140,     8,     0,   139,   162,     0,   140,   162,     0,
   135,   160,     0,   136,   160,     0,   161,     0,   139,   173,
     0,   139,   163,     0,   140,   163,     0,   135,   161,     0,
   136,   161,     0,   141,     8,     0,   142,     8,     0,   141,
   162,     0,   142,   162,     0,   137,   160,     0,   138,   160,
     0,   141,   173,     0,   141,   163,     0,   142,   163,     0,
   137,   161,     0,   138,   161,     0,   178,     0,   143,     8,
     0,   144,     8,     0,   135,   178,     0,   136,   178,     0,
   143,   178,     0,   144,   178,     0,   143,   173,     0,   145,
     8,     0,   146,     8,     0,   137,   178,     0,   138,   178,
     0,   145,   178,     0,   146,   178,     0,   145,   173,     0,
   147,     8,     0,   148,     8,     0,   147,   162,     0,   148,
   162,     0,   143,   160,     0,   144,   160,     0,   139,   178,
     0,   140,   178,     0,   147,   178,     0,   148,   178,     0,
   147,   173,     0,   147,   163,     0,   148,   163,     0,   143,
   161,     0,   144,   161,     0,   149,     8,     0,   150,     8,
     0,   149,   162,     0,   150,   162,     0,   145,   160,     0,
   146,   160,     0,   141,   178,     0,   142,   178,     0,   149,
   178,     0,   150,   178,     0,   149,   173,     0,   149,   163,
     0,   150,   163,     0,   145,   161,     0,   146,   161,     0,
   139,     0,   140,     0,   141,     0,   142,     0,   147,     0,
   148,     0,   149,     0,   150,     0,   135,     0,   136,     0,
   137,     0,   138,     0,   143,     0,   144,     0,   145,     0,
   146,     0,   139,     0,   140,     0,   147,     0,   148,     0,
   135,     0,   136,     0,   143,     0,   144,     0,   139,     0,
   140,     0,   141,     0,   142,     0,   135,     0,   136,     0,
   137,     0,   138,     0,   139,     0,   140,     0,   141,     0,
   142,     0,   135,     0,   136,     0,   137,     0,   138,     0,
   135,     0,   136,     0,   137,     0,   138,     0,   139,     0,
   140,     0,   141,     0,   142,     0,   143,     0,   144,     0,
   145,     0,   146,     0,   147,     0,   148,     0,   149,     0,
   150,     0,     0,   156,     0,   162,     0,   164,     0,   163,
     0,     7,     0,   209,     0,   204,     0,     4,     0,    76,
   311,     0,    81,   311,     0,   312,     0,   115,    68,   109,
    84,     0,   115,    68,   221,    84,     0,   168,     0,   165,
    89,   133,   168,     0,   170,     0,   166,    89,   133,   170,
     0,     0,    28,    68,    10,    84,     0,     0,   195,   167,
   172,    45,   169,   179,     0,   195,   167,   172,     0,     0,
   200,   167,   172,    45,   171,   179,     0,   200,   167,   172,
     0,     0,   173,     0,   174,     0,   173,   174,     0,    31,
    68,    68,   175,    84,    84,     0,   176,     0,   175,    89,
   176,     0,     0,   177,     0,   177,    68,     3,    84,     0,
   177,    68,     3,    89,   111,    84,     0,   177,    68,   110,
    84,     0,   107,     0,   178,     0,     7,     0,     8,     0,
     6,     0,     5,     0,   117,     0,     0,    90,   180,   181,
    86,     0,     1,     0,     0,   182,   210,     0,   183,     0,
   182,    89,   183,     0,   187,    45,   185,     0,   188,   185,
     0,     0,   107,    47,   184,   185,     0,   185,     0,     0,
    90,   186,   181,    86,     0,   117,     0,     1,     0,   188,
     0,   187,   188,     0,    67,   107,     0,    69,   117,    11,
   117,    91,     0,    69,   117,    91,     0,     0,     0,   195,
   190,   126,   191,   255,   256,   249,     0,     0,     0,   200,
   193,   126,   194,   255,   256,   249,     0,   196,     0,   200,
     0,    68,   172,   196,    84,     0,   196,    68,   289,     0,
   196,   229,     0,    59,   159,   196,     0,     4,     0,    81,
     0,   198,     0,   199,     0,   198,    68,   289,     0,   198,
   229,     0,     4,     0,    81,     0,   199,    68,   289,     0,
   199,   229,     0,    59,   159,   198,     0,    59,   159,   199,
     0,    68,   172,   199,    84,     0,   200,    68,   289,     0,
    68,   172,   200,    84,     0,    59,   159,   200,     0,   200,
   229,     0,     3,     0,    14,     0,    14,   173,     0,    15,
     0,    15,   173,     0,    13,     0,    13,   173,     0,     0,
   201,   107,    90,   205,   212,    86,   172,     0,   201,    90,
   212,    86,   172,     0,     0,   202,   107,    90,   206,   212,
    86,   172,     0,   202,    90,   212,    86,   172,     0,     0,
   203,   107,    90,   207,   219,   211,    86,   172,     0,     0,
   203,    90,   208,   219,   211,    86,   172,     0,   201,   107,
     0,   202,   107,     0,   203,   107,     0,     0,    89,     0,
     0,    89,     0,   213,     0,   213,   214,     0,     0,   213,
   214,    85,     0,   213,    85,     0,    74,    68,    76,    84,
     0,   155,   132,   215,     0,   155,   132,   255,   256,     0,
   156,   132,   216,     0,   156,     0,     1,     0,   294,   214,
     0,   217,     0,   215,    89,   133,   217,     0,   218,     0,
   216,    89,   133,   218,     0,   255,   256,   195,   172,     0,
   255,   256,   195,    47,   117,   172,     0,   255,   256,    47,
   117,   172,     0,   255,   256,   200,   172,     0,   255,   256,
   200,    47,   117,   172,     0,   255,   256,    47,   117,   172,
     0,   220,     0,   219,    89,   220,     0,     1,     0,   107,
     0,   107,    45,   117,     0,     0,   157,   222,   223,     0,
     0,   225,     0,     0,   225,     0,   226,   173,     0,   227,
     0,   226,     0,   228,     0,    59,   159,   226,     0,    59,
   159,     0,    59,   159,   227,     0,    68,   172,   225,    84,
     0,   228,    68,   279,     0,   228,   229,     0,    68,   279,
     0,   229,     0,    69,   159,   109,    91,     0,    69,   159,
    91,     0,    69,   159,    59,    91,     0,    69,     6,   159,
   109,    91,     0,    69,   156,     6,   109,    91,     0,   231,
     0,   232,     0,   233,     0,   234,     0,   259,     0,   231,
   259,     0,   232,   259,     0,   233,   259,     0,   234,   259,
     0,   131,     0,   231,   131,     0,   232,   131,     0,   234,
   131,     0,   260,     0,   231,   260,     0,   232,   260,     0,
   233,   260,     0,   234,   260,     0,   236,     0,   235,   236,
     0,   231,     0,   232,     0,   233,     0,   234,     0,     1,
    85,     0,     0,     0,     0,     0,     0,   242,     0,   243,
     0,   242,   243,     0,    33,   293,    85,     0,   249,     0,
     1,   249,     0,    90,     0,    86,     0,   237,   241,   247,
    86,   238,     0,   230,     0,     1,     0,    68,    90,     0,
   245,   246,     0,   251,   258,     0,   251,     1,     0,     0,
    16,   252,    68,   109,    84,     0,     0,    19,   254,   258,
    18,     0,     0,     0,   259,     0,   260,   257,     0,   239,
   257,   240,     0,   255,   256,   271,     0,   255,   256,   272,
     0,     0,   250,    17,   262,   258,     0,   250,     0,   250,
    17,     1,     0,     0,     0,    18,   263,    68,   109,    84,
   264,   258,     0,   253,    68,   109,    84,    85,     0,   253,
     1,     0,     0,     0,     0,     0,    20,   265,    68,   270,
   266,   274,    85,   267,   274,    84,   268,   258,     0,     0,
    21,    68,   109,    84,   269,   258,     0,   274,    85,     0,
   134,     0,   249,     0,   109,    85,     0,   239,   261,   240,
     0,    24,    85,     0,    25,    85,     0,    26,    85,     0,
    26,   109,    85,     0,    28,   273,    68,   109,    84,    85,
     0,    28,   273,    68,   109,    47,   275,    84,    85,     0,
    28,   273,    68,   109,    47,   275,    47,   275,    84,    85,
     0,    28,   273,    68,   109,    47,   275,    47,   275,    47,
   278,    84,    85,     0,    27,   107,    85,     0,    27,    59,
   109,    85,     0,    85,     0,    22,   117,    47,     0,    22,
   117,    11,   117,    47,     0,    23,    47,     0,   107,   255,
   256,    47,   172,     0,     0,     8,     0,     0,   109,     0,
     0,   276,     0,   277,     0,   276,    89,   277,     0,    10,
    68,   109,    84,     0,    69,   107,    91,    10,    68,   109,
    84,     0,    10,     0,   278,    89,    10,     0,     0,   172,
   280,   281,     0,   284,    84,     0,     0,     0,   285,    85,
   282,   172,   283,   281,     0,     1,    84,     0,     0,    11,
     0,   285,     0,   285,    89,    11,     0,   287,     0,   285,
    89,   286,     0,   151,   132,   197,   172,     0,   151,   132,
   200,   172,     0,   151,   132,   224,     0,   152,   132,   200,
   172,     0,   152,   132,   224,     0,   153,   288,   197,   172,
     0,   153,   288,   200,   172,     0,   153,   288,   224,     0,
   154,   288,   200,   172,     0,   154,   288,   224,     0,   132,
     0,     0,   172,   290,   291,     0,   281,     0,   292,    84,
     0,     3,     0,   292,    89,     3,     0,   107,     0,   293,
    89,   107,     0,    32,     0,   299,     0,   297,     0,   298,
     0,   309,     0,   320,     0,    72,     0,   107,     0,   296,
    89,   107,     0,    82,   296,    85,     0,    83,   107,   107,
    85,     0,     0,     0,    70,   107,   311,    90,   300,   313,
    86,   301,   324,    72,     0,     0,    70,   107,   311,   302,
   324,    72,     0,     0,     0,    70,   107,    47,   107,   311,
    90,   303,   313,    86,   304,   324,    72,     0,     0,    70,
   107,    47,   107,   311,   305,   324,    72,     0,     0,    71,
   107,    90,   306,   313,    86,     0,    71,   107,     0,     0,
    71,   107,    47,   107,    90,   307,   313,    86,     0,    71,
   107,    47,   107,     0,     0,    70,   107,    68,   107,    84,
   311,   308,   324,    72,     0,    71,   107,    68,   107,    84,
     0,     0,    80,   107,   311,   310,   324,    72,     0,    80,
   296,    85,     0,     0,   312,     0,    54,   296,    54,     0,
   313,   314,   315,     0,   315,     0,    78,     0,    79,     0,
    77,     0,     0,   315,   316,    85,     0,   315,    85,     0,
   155,   132,   317,     0,   156,   132,   317,     0,     1,     0,
     0,   318,     0,   317,    89,   133,   318,     0,   195,     0,
   195,    47,   117,     0,    47,   117,     0,    57,     0,    58,
     0,     0,     0,     0,   319,   321,   332,   322,   333,   323,
   244,     0,     0,     0,   325,   326,     0,   329,     0,    99,
     0,   326,   329,     0,     0,   326,   327,    99,     0,    85,
     0,     1,     0,     0,     0,   319,   330,   332,   331,   328,
     0,    68,   221,    84,   341,     0,   341,     0,    68,   221,
    84,   342,   339,     0,   342,   339,     0,     0,    85,   334,
     0,     0,   335,     0,   336,     0,   236,     0,   335,   336,
     0,   336,   236,     0,   151,   132,   337,    85,     0,   151,
    85,     0,   152,    85,     0,   338,     0,   337,    89,   338,
     0,   197,   172,     0,   200,   172,     0,   224,     0,     0,
    89,    11,     0,     0,    89,   340,   284,     0,   343,     0,
   345,     0,   342,   345,     0,     3,     0,     4,     0,    76,
     0,    81,     0,   344,     0,    13,     0,    14,     0,    15,
     0,    16,     0,    17,     0,    18,     0,    19,     0,    20,
     0,    21,     0,    22,     0,    23,     0,    24,     0,    25,
     0,    26,     0,    27,     0,    28,     0,    12,     0,    29,
     0,    30,     0,     7,     0,     8,     0,   343,    47,    68,
   221,    84,   107,     0,   343,    47,   107,     0,    47,    68,
   221,    84,   107,     0,    47,   107,     0,   343,     0,   347,
     0,   349,     0,   347,   349,     0,   111,     0,   343,    47,
   348,     0,    47,   348,     0,   109,     0,    76,     0,    69,
   350,   346,    91,     0,   343,     0,   353,     0,   354,     0,
   353,   354,     0,   343,    47,     0,    47,     0,    73,    68,
   352,    84,     0,    80,    68,   107,    84,     0,    75,    68,
   221,    84,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   344,   349,   367,   369,   369,   370,   372,   377,   379,   380,
   381,   389,   393,   401,   403,   405,   407,   408,   409,   414,
   421,   422,   427,   429,   435,   436,   441,   443,   449,   450,
   455,   459,   461,   462,   463,   466,   468,   470,   473,   475,
   477,   479,   483,   487,   490,   493,   496,   500,   502,   505,
   508,   512,   514,   520,   523,   526,   529,   531,   535,   539,
   543,   547,   549,   553,   555,   557,   559,   561,   563,   565,
   567,   569,   571,   573,   575,   577,   579,   584,   586,   591,
   593,   597,   601,   603,   611,   614,   621,   632,   639,   640,
   642,   644,   648,   657,   662,   664,   680,   687,   689,   692,
   702,   712,   714,   721,   730,   732,   734,   736,   738,   740,
   742,   748,   751,   755,   762,   770,   776,   781,   783,   784,
   785,   792,   795,   797,   800,   808,   817,   827,   832,   835,
   837,   839,   841,   843,   899,   903,   906,   911,   917,   921,
   926,   930,   935,   939,   942,   945,   948,   951,   954,   959,
   963,   966,   969,   972,   975,   980,   984,   987,   990,   993,
   996,  1001,  1005,  1008,  1011,  1014,  1019,  1023,  1026,  1029,
  1035,  1041,  1047,  1055,  1061,  1065,  1068,  1074,  1080,  1086,
  1094,  1100,  1104,  1107,  1110,  1113,  1116,  1119,  1125,  1131,
  1137,  1145,  1149,  1152,  1155,  1158,  1163,  1167,  1170,  1173,
  1176,  1179,  1182,  1188,  1194,  1200,  1208,  1212,  1215,  1218,
  1221,  1227,  1229,  1230,  1231,  1232,  1233,  1234,  1235,  1238,
  1240,  1241,  1242,  1243,  1244,  1245,  1246,  1249,  1251,  1252,
  1253,  1256,  1258,  1259,  1260,  1263,  1265,  1266,  1267,  1270,
  1272,  1273,  1274,  1277,  1279,  1280,  1281,  1282,  1283,  1284,
  1285,  1288,  1290,  1291,  1292,  1293,  1294,  1295,  1296,  1297,
  1298,  1299,  1300,  1301,  1302,  1303,  1304,  1308,  1311,  1336,
  1338,  1341,  1345,  1348,  1351,  1355,  1360,  1362,  1367,  1369,
  1371,  1377,  1379,  1382,  1384,  1387,  1390,  1394,  1401,  1403,
  1410,  1417,  1419,  1426,  1429,  1433,  1436,  1440,  1445,  1448,
  1452,  1455,  1457,  1459,  1461,  1468,  1470,  1471,  1472,  1475,
  1477,  1482,  1484,  1486,  1488,  1493,  1497,  1500,  1502,  1507,
  1511,  1514,  1519,  1520,  1523,  1526,  1528,  1530,  1533,  1535,
  1538,  1541,  1545,  1549,  1564,  1571,  1580,  1595,  1602,  1614,
  1616,  1621,  1624,  1629,  1631,  1633,  1634,  1641,  1643,  1646,
  1652,  1654,  1655,  1658,  1664,  1666,  1668,  1670,  1677,  1683,
  1685,  1687,  1689,  1692,  1695,  1699,  1702,  1706,  1709,  1719,
  1724,  1726,  1730,  1732,  1734,  1738,  1740,  1743,  1745,  1750,
  1753,  1755,  1763,  1765,  1768,  1770,  1775,  1778,  1783,  1785,
  1787,  1791,  1806,  1810,  1820,  1823,  1828,  1830,  1835,  1837,
  1841,  1843,  1847,  1851,  1855,  1860,  1864,  1868,  1878,  1880,
  1885,  1890,  1893,  1897,  1902,  1905,  1908,  1911,  1916,  1920,
  1926,  1928,  1931,  1933,  1937,  1940,  1944,  1947,  1949,  1951,
  1953,  1959,  1962,  1964,  1966,  1969,  1979,  1981,  1982,  1986,
  1989,  1991,  1992,  1993,  1994,  1997,  1999,  2002,  2003,  2006,
  2008,  2009,  2010,  2011,  2014,  2016,  2019,  2021,  2022,  2023,
  2026,  2029,  2038,  2043,  2061,  2076,  2078,  2083,  2085,  2088,
  2102,  2105,  2108,  2112,  2114,  2121,  2123,  2126,  2144,  2151,
  2157,  2160,  2171,  2182,  2194,  2202,  2208,  2214,  2216,  2220,
  2226,  2240,  2249,  2254,  2258,  2270,  2280,  2283,  2289,  2290,
  2293,  2295,  2299,  2302,  2306,  2309,  2310,  2314,  2317,  2320,
  2325,  2328,  2331,  2335,  2338,  2341,  2344,  2347,  2351,  2355,
  2360,  2364,  2376,  2382,  2390,  2393,  2396,  2399,  2414,  2418,
  2422,  2425,  2430,  2432,  2435,  2437,  2441,  2444,  2450,  2453,
  2462,  2468,  2473,  2475,  2484,  2487,  2488,  2494,  2496,  2506,
  2510,  2514,  2517,  2523,  2529,  2534,  2537,  2543,  2550,  2556,
  2561,  2564,  2570,  2575,  2584,  2590,  2595,  2597,  2614,  2617,
  2622,  2625,  2629,  2640,  2642,  2643,  2644,  2645,  2646,  2660,
  2663,  2667,  2674,  2681,  2688,  2693,  2699,  2706,  2712,  2718,
  2723,  2729,  2736,  2742,  2748,  2754,  2762,  2768,  2774,  2782,
  2789,  2795,  2804,  2811,  2820,  2826,  2831,  2834,  2844,  2846,
  2849,  2851,  2852,  2855,  2860,  2861,  2878,  2882,  2885,  2889,
  2892,  2893,  2896,  2904,  2910,  2919,  2922,  2926,  2934,  2943,
  2947,  2956,  2958,  2959,  2961,  2963,  2964,  2965,  2966,  2968,
  2970,  2973,  2980,  2989,  2991,  2997,  3002,  3007,  3016,  3018,
  3024,  3026,  3029,  3031,  3032,  3033,  3036,  3039,  3041,  3045,
  3048,  3055,  3060,  3064,  3068,  3073,  3078,  3083,  3090,  3094,
  3097,  3103,  3105,  3106,  3107,  3108,  3111,  3112,  3112,  3112,
  3112,  3112,  3112,  3112,  3113,  3113,  3113,  3113,  3113,  3113,
  3114,  3114,  3114,  3114,  3114,  3115,  3115,  3118,  3124,  3129,
  3134,  3140,  3142,  3145,  3147,  3154,  3166,  3171,  3177,  3179,
  3185,  3190,  3192,  3195,  3197,  3203,  3208,  3214,  3221,  3230
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","IDENTIFIER",
"TYPENAME","SCSPEC","STATIC","TYPESPEC","TYPE_QUAL","CONSTANT","STRING","ELLIPSIS",
"SIZEOF","ENUM","STRUCT","UNION","IF","ELSE","WHILE","DO","FOR","SWITCH","CASE",
"DEFAULT","BREAK","CONTINUE","RETURN","GOTO","ASM_KEYWORD","TYPEOF","ALIGNOF",
"ATTRIBUTE","EXTENSION","LABEL","REALPART","IMAGPART","VA_ARG","CHOOSE_EXPR",
"TYPES_COMPATIBLE_P","PTR_VALUE","PTR_BASE","PTR_EXTENT","STRING_FUNC_NAME",
"VAR_FUNC_NAME","ASSIGN","'='","'?'","':'","OROR","ANDAND","'|'","'^'","'&'",
"EQCOMPARE","ARITHCOMPARE","LSHIFT","RSHIFT","'+'","'-'","'*'","'/'","'%'","UNARY",
"PLUSPLUS","MINUSMINUS","HYPERUNARY","POINTSAT","'.'","'('","'['","INTERFACE",
"IMPLEMENTATION","END","SELECTOR","DEFS","ENCODE","CLASSNAME","PUBLIC","PRIVATE",
"PROTECTED","PROTOCOL","OBJECTNAME","CLASS","ALIAS","')'","';'","'}'","'~'",
"'!'","','","'{'","']'","'@'","program","extdefs","@1","@2","extdef","extdef_1",
"datadef","fndef","@3","@4","@5","@6","@7","@8","identifier","unop","expr","exprlist",
"nonnull_exprlist","unary_expr","sizeof","alignof","typeof","cast_expr","expr_no_commas",
"@9","@10","@11","@12","@13","primary","@14","objc_string","old_style_parm_decls",
"old_style_parm_decls_1","lineno_datadecl","datadecls","datadecl","lineno_decl",
"setspecs","maybe_resetattrs","decl","declspecs_nosc_nots_nosa_noea","declspecs_nosc_nots_nosa_ea",
"declspecs_nosc_nots_sa_noea","declspecs_nosc_nots_sa_ea","declspecs_nosc_ts_nosa_noea",
"declspecs_nosc_ts_nosa_ea","declspecs_nosc_ts_sa_noea","declspecs_nosc_ts_sa_ea",
"declspecs_sc_nots_nosa_noea","declspecs_sc_nots_nosa_ea","declspecs_sc_nots_sa_noea",
"declspecs_sc_nots_sa_ea","declspecs_sc_ts_nosa_noea","declspecs_sc_ts_nosa_ea",
"declspecs_sc_ts_sa_noea","declspecs_sc_ts_sa_ea","declspecs_ts","declspecs_nots",
"declspecs_ts_nosa","declspecs_nots_nosa","declspecs_nosc_ts","declspecs_nosc_nots",
"declspecs_nosc","declspecs","maybe_type_quals_attrs","typespec_nonattr","typespec_attr",
"typespec_reserved_nonattr","typespec_reserved_attr","typespec_nonreserved_nonattr",
"initdecls","notype_initdecls","maybeasm","initdcl","@15","notype_initdcl","@16",
"maybe_attribute","attributes","attribute","attribute_list","attrib","any_word",
"scspec","init","@17","initlist_maybe_comma","initlist1","initelt","@18","initval",
"@19","designator_list","designator","nested_function","@20","@21","notype_nested_function",
"@22","@23","declarator","after_type_declarator","parm_declarator","parm_declarator_starttypename",
"parm_declarator_nostarttypename","notype_declarator","struct_head","union_head",
"enum_head","structsp_attr","@24","@25","@26","@27","structsp_nonattr","maybecomma",
"maybecomma_warn","component_decl_list","component_decl_list2","component_decl",
"components","components_notype","component_declarator","component_notype_declarator",
"enumlist","enumerator","typename","@28","absdcl","absdcl_maybe_attribute","absdcl1",
"absdcl1_noea","absdcl1_ea","direct_absdcl1","array_declarator","stmts_and_decls",
"lineno_stmt_decl_or_labels_ending_stmt","lineno_stmt_decl_or_labels_ending_decl",
"lineno_stmt_decl_or_labels_ending_label","lineno_stmt_decl_or_labels_ending_error",
"lineno_stmt_decl_or_labels","errstmt","pushlevel","poplevel","c99_block_start",
"c99_block_end","maybe_label_decls","label_decls","label_decl","compstmt_or_error",
"compstmt_start","compstmt_nostart","compstmt_contents_nonempty","compstmt_primary_start",
"compstmt","simple_if","if_prefix","@29","do_stmt_start","@30","save_filename",
"save_lineno","lineno_labeled_stmt","c99_block_lineno_labeled_stmt","lineno_stmt",
"lineno_label","select_or_iter_stmt","@31","@32","@33","@34","@35","@36","@37",
"@38","for_init_stmt","stmt","label","maybe_type_qual","xexpr","asm_operands",
"nonnull_asm_operands","asm_operand","asm_clobbers","parmlist","@39","parmlist_1",
"@40","@41","parmlist_2","parms","parm","firstparm","setspecs_fp","parmlist_or_identifiers",
"@42","parmlist_or_identifiers_1","identifiers","identifiers_or_typenames","extension",
"objcdef","identifier_list","classdecl","aliasdecl","classdef","@43","@44","@45",
"@46","@47","@48","@49","@50","@51","protocoldef","@52","protocolrefs","non_empty_protocolrefs",
"ivar_decl_list","visibility_spec","ivar_decls","ivar_decl","ivars","ivar_declarator",
"methodtype","methoddef","@53","@54","@55","methodprotolist","@56","methodprotolist2",
"@57","semi_or_error","methodproto","@58","@59","methoddecl","optarglist","myxdecls",
"mydecls","mydecl","myparms","myparm","optparmlist","@60","unaryselector","keywordselector",
"selector","reservedwords","keyworddecl","messageargs","keywordarglist","keywordexpr",
"keywordarg","receiver","objcmessageexpr","selectorarg","keywordnamelist","keywordname",
"objcselectorexpr","objcprotocolexpr","objcencodeexpr", NULL
};
#endif

static const short yyr1[] = {     0,
    93,    93,    95,    94,    96,    94,    97,    98,    98,    98,
    98,    98,    99,    99,    99,    99,    99,    99,    99,   101,
   102,   100,   100,   103,   104,   100,   100,   105,   106,   100,
   100,   107,   107,   107,   107,   108,   108,   108,   108,   108,
   108,   108,   109,   110,   110,   111,   111,   112,   112,   112,
   112,   112,   112,   112,   112,   112,   112,   112,   113,   114,
   115,   116,   116,   117,   117,   117,   117,   117,   117,   117,
   117,   117,   117,   117,   117,   117,   118,   117,   119,   117,
   120,   121,   117,   122,   117,   117,   117,   123,   123,   123,
   123,   124,   123,   123,   123,   123,   123,   123,   123,   123,
   123,   123,   123,   123,   123,   123,   123,   123,   123,   123,
   123,   125,   125,   126,   127,   127,   128,   129,   129,   129,
   129,   130,   130,   130,   130,   131,   132,   133,   134,   134,
   134,   134,   134,   134,   135,   135,   135,   136,   137,   137,
   138,   138,   139,   139,   139,   139,   139,   139,   139,   140,
   140,   140,   140,   140,   140,   141,   141,   141,   141,   141,
   141,   142,   142,   142,   142,   142,   143,   143,   143,   143,
   143,   143,   143,   144,   145,   145,   145,   145,   145,   145,
   146,   147,   147,   147,   147,   147,   147,   147,   147,   147,
   147,   148,   148,   148,   148,   148,   149,   149,   149,   149,
   149,   149,   149,   149,   149,   149,   150,   150,   150,   150,
   150,   151,   151,   151,   151,   151,   151,   151,   151,   152,
   152,   152,   152,   152,   152,   152,   152,   153,   153,   153,
   153,   154,   154,   154,   154,   155,   155,   155,   155,   156,
   156,   156,   156,   157,   157,   157,   157,   157,   157,   157,
   157,   158,   158,   158,   158,   158,   158,   158,   158,   158,
   158,   158,   158,   158,   158,   158,   158,   159,   159,   160,
   160,   161,   162,   162,   163,   164,   164,   164,   164,   164,
   164,   165,   165,   166,   166,   167,   167,   169,   168,   168,
   171,   170,   170,   172,   172,   173,   173,   174,   175,   175,
   176,   176,   176,   176,   176,   177,   177,   177,   177,   178,
   178,   179,   180,   179,   179,   181,   181,   182,   182,   183,
   183,   184,   183,   183,   186,   185,   185,   185,   187,   187,
   188,   188,   188,   190,   191,   189,   193,   194,   192,   195,
   195,   196,   196,   196,   196,   196,   196,   197,   197,   198,
   198,   198,   198,   199,   199,   199,   199,   199,   200,   200,
   200,   200,   200,   201,   201,   202,   202,   203,   203,   205,
   204,   204,   206,   204,   204,   207,   204,   208,   204,   209,
   209,   209,   210,   210,   211,   211,   212,   212,   213,   213,
   213,   213,   214,   214,   214,   214,   214,   214,   215,   215,
   216,   216,   217,   217,   217,   218,   218,   218,   219,   219,
   219,   220,   220,   222,   221,   223,   223,   224,   224,   224,
   225,   225,   226,   226,   227,   227,   228,   228,   228,   228,
   228,   229,   229,   229,   229,   229,   230,   230,   230,   230,
   231,   231,   231,   231,   231,   232,   232,   232,   232,   233,
   233,   233,   233,   233,   234,   234,   235,   235,   235,   235,
   236,   237,   238,   239,   240,   241,   241,   242,   242,   243,
   244,   244,   245,   246,   246,   247,   247,   248,   249,   250,
   250,   252,   251,   254,   253,   255,   256,   257,   257,   258,
   259,   260,   262,   261,   261,   261,   263,   264,   261,   261,
   261,   265,   266,   267,   268,   261,   269,   261,   270,   270,
   271,   271,   271,   271,   271,   271,   271,   271,   271,   271,
   271,   271,   271,   271,   272,   272,   272,   272,   273,   273,
   274,   274,   275,   275,   276,   276,   277,   277,   278,   278,
   280,   279,   281,   282,   283,   281,   281,   284,   284,   284,
   284,   285,   285,   286,   286,   286,   286,   286,   287,   287,
   287,   287,   287,   288,   290,   289,   291,   291,   292,   292,
   293,   293,   294,   295,   295,   295,   295,   295,   295,   296,
   296,   297,   298,   300,   301,   299,   302,   299,   303,   304,
   299,   305,   299,   306,   299,   299,   307,   299,   299,   308,
   299,   299,   310,   309,   309,   311,   311,   312,   313,   313,
   314,   314,   314,   315,   315,   315,   316,   316,   316,   317,
   317,   317,   318,   318,   318,   319,   319,   321,   322,   323,
   320,   324,   325,   324,   326,   326,   326,   327,   326,   328,
   328,   330,   331,   329,   332,   332,   332,   332,   333,   333,
   334,   334,   335,   335,   335,   335,   336,   336,   336,   337,
   337,   338,   338,   338,   339,   339,   340,   339,   341,   342,
   342,   343,   343,   343,   343,   343,   344,   344,   344,   344,
   344,   344,   344,   344,   344,   344,   344,   344,   344,   344,
   344,   344,   344,   344,   344,   344,   344,   345,   345,   345,
   345,   346,   346,   347,   347,   348,   349,   349,   350,   350,
   351,   352,   352,   353,   353,   354,   354,   355,   356,   357
};

static const short yyr2[] = {     0,
     0,     1,     0,     2,     0,     3,     1,     1,     1,     1,
     5,     2,     3,     4,     4,     2,     2,     2,     1,     0,
     0,     9,     4,     0,     0,     9,     4,     0,     0,     8,
     3,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     0,     1,     1,     3,     1,     2,     2,
     2,     2,     2,     4,     2,     4,     2,     2,     1,     1,
     1,     1,     4,     1,     3,     3,     3,     3,     3,     3,
     3,     3,     3,     3,     3,     3,     0,     4,     0,     4,
     0,     0,     7,     0,     5,     3,     3,     1,     1,     1,
     1,     0,     7,     3,     3,     3,     3,     4,     6,     8,
     6,     4,     3,     3,     2,     2,     1,     1,     1,     1,
     1,     2,     3,     1,     0,     1,     3,     1,     1,     2,
     2,     4,     4,     2,     2,     3,     0,     1,     4,     4,
     3,     3,     2,     2,     1,     2,     2,     2,     2,     2,
     1,     2,     1,     2,     2,     2,     2,     2,     2,     1,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     1,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     0,     1,     1,
     1,     1,     1,     1,     1,     1,     2,     2,     1,     4,
     4,     1,     4,     1,     4,     0,     4,     0,     6,     3,
     0,     6,     3,     0,     1,     1,     2,     6,     1,     3,
     0,     1,     4,     6,     4,     1,     1,     1,     1,     1,
     1,     1,     0,     4,     1,     0,     2,     1,     3,     3,
     2,     0,     4,     1,     0,     4,     1,     1,     1,     2,
     2,     5,     3,     0,     0,     7,     0,     0,     7,     1,
     1,     4,     3,     2,     3,     1,     1,     1,     1,     3,
     2,     1,     1,     3,     2,     3,     3,     4,     3,     4,
     3,     2,     1,     1,     2,     1,     2,     1,     2,     0,
     7,     5,     0,     7,     5,     0,     8,     0,     7,     2,
     2,     2,     0,     1,     0,     1,     1,     2,     0,     3,
     2,     4,     3,     4,     3,     1,     1,     2,     1,     4,
     1,     4,     4,     6,     5,     4,     6,     5,     1,     3,
     1,     1,     3,     0,     3,     0,     1,     0,     1,     2,
     1,     1,     1,     3,     2,     3,     4,     3,     2,     2,
     1,     4,     3,     4,     5,     5,     1,     1,     1,     1,
     1,     2,     2,     2,     2,     1,     2,     2,     2,     1,
     2,     2,     2,     2,     1,     2,     1,     1,     1,     1,
     2,     0,     0,     0,     0,     0,     1,     1,     2,     3,
     1,     2,     1,     1,     5,     1,     1,     2,     2,     2,
     2,     0,     5,     0,     4,     0,     0,     1,     2,     3,
     3,     3,     0,     4,     1,     3,     0,     0,     7,     5,
     2,     0,     0,     0,     0,    12,     0,     6,     2,     1,
     1,     2,     3,     2,     2,     2,     3,     6,     8,    10,
    12,     3,     4,     1,     3,     5,     2,     5,     0,     1,
     0,     1,     0,     1,     1,     3,     4,     7,     1,     3,
     0,     3,     2,     0,     0,     6,     2,     0,     1,     1,
     3,     1,     3,     4,     4,     3,     4,     3,     4,     4,
     3,     4,     3,     1,     0,     3,     1,     2,     1,     3,
     1,     3,     1,     1,     1,     1,     1,     1,     1,     1,
     3,     3,     4,     0,     0,    10,     0,     6,     0,     0,
    12,     0,     8,     0,     6,     2,     0,     8,     4,     0,
     9,     5,     0,     6,     3,     0,     1,     3,     3,     1,
     1,     1,     1,     0,     3,     2,     3,     3,     1,     0,
     1,     4,     1,     3,     2,     1,     1,     0,     0,     0,
     7,     0,     0,     2,     1,     1,     2,     0,     3,     1,
     1,     0,     0,     5,     4,     1,     5,     2,     0,     2,
     0,     1,     1,     1,     2,     2,     4,     2,     2,     1,
     3,     2,     2,     1,     0,     2,     0,     3,     1,     1,
     2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     6,     3,     5,
     2,     1,     1,     1,     2,     1,     3,     2,     1,     1,
     4,     1,     1,     1,     2,     2,     1,     4,     4,     4
};

static const short yydefact[] = {     3,
     5,     0,     0,     0,   276,   311,   310,   273,   135,   368,
   364,   366,     0,    61,     0,   573,     0,   626,   627,     0,
     0,   579,   606,     0,   606,     0,     0,    19,     4,     7,
     9,     8,     0,     0,   220,   221,   222,   223,   212,   213,
   214,   215,   224,   225,   226,   227,   216,   217,   218,   219,
   127,   127,     0,   143,   150,   270,   272,   271,   141,   296,
   167,     0,     0,     0,   275,   274,     0,    10,   575,   576,
   574,   577,   279,   628,   578,     6,    17,    18,   369,   365,
   367,     0,     0,    32,    33,    35,    34,   580,     0,   606,
   596,   277,   607,   606,     0,   278,     0,     0,     0,   363,
   268,   294,     0,   284,     0,   136,   148,   154,   138,   170,
   137,   149,   155,   171,   139,   160,   165,   142,   177,   140,
   161,   166,   178,   144,   146,   152,   151,   188,   145,   147,
   153,   189,   156,   158,   163,   162,   203,   157,   159,   164,
   204,   168,   186,   195,   174,   172,   169,   187,   196,   173,
   175,   201,   210,   181,   179,   176,   202,   211,   180,   182,
   184,   193,   192,   190,   183,   185,   194,   191,   197,   199,
   208,   207,   205,   198,   200,   209,   206,     0,     0,    16,
   297,   389,   380,   389,   381,   378,   382,    12,     0,    88,
    89,    90,    59,    60,     0,     0,     0,     0,     0,    91,
     0,    36,    38,    37,     0,    39,    40,     0,     0,     0,
     0,     0,    41,    42,     0,     0,     0,    43,    62,     0,
     0,    64,    46,    48,   111,     0,     0,   107,   108,   109,
   110,   301,   608,     0,     0,     0,   587,     0,     0,   594,
   603,   605,   582,     0,     0,   248,   249,   250,   251,   244,
   245,   246,   247,   414,     0,   240,   241,   242,   243,   269,
     0,     0,   295,    13,   294,    31,     0,   294,   268,     0,
   294,   362,   346,   268,   294,   347,     0,   282,     0,   340,
   341,     0,     0,     0,     0,     0,   370,     0,   373,     0,
   376,   672,   673,   696,   697,   693,   677,   678,   679,   680,
   681,   682,   683,   684,   685,   686,   687,   688,   689,   690,
   691,   692,   694,   695,     0,     0,   674,   675,   629,   646,
   665,   669,   676,   670,    57,    58,     0,     0,     0,    52,
    49,     0,   478,     0,     0,   710,   709,     0,     0,     0,
     0,   112,    51,     0,     0,     0,    53,     0,    55,     0,
     0,    81,    79,    77,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,   105,   106,     0,     0,
    44,     0,     0,     0,   474,   466,     0,    50,   308,   309,
   306,     0,   299,   302,   307,   581,   606,     0,   584,   633,
   599,     0,   614,   633,   583,   280,   416,   281,   361,     0,
     0,   128,     0,   565,   359,   268,   269,     0,     0,    29,
   114,     0,   486,   119,   487,   293,     0,     0,    15,   294,
    23,     0,   294,   294,   344,    14,    27,     0,     0,   294,
   397,   391,   240,   241,   242,   243,   236,   237,   238,   239,
   127,   127,   388,     0,   389,   294,   389,   411,   412,   385,
   409,     0,     0,   701,     0,   649,   667,   648,     0,   671,
     0,     0,     0,     0,    95,    94,     0,     0,   702,     0,
   703,   704,   717,   712,     0,   713,   714,     0,     0,    11,
    47,     0,     0,    87,    86,     0,     0,     0,     0,    75,
    76,    74,    73,    72,    70,    71,    65,    66,    67,    68,
    69,   104,   103,     0,    45,     0,   113,    97,     0,     0,
   467,   468,    96,     0,   301,    44,   592,   606,   614,     0,
     0,   597,   602,     0,     0,     0,   268,   294,   415,   417,
   422,   421,   423,   431,   360,   285,   286,     0,     0,     0,
     0,     0,   433,     0,   461,   486,   121,   120,     0,   291,
   345,     0,     0,    21,   290,   343,    25,     0,   372,   486,
   486,   390,   398,     0,   375,     0,     0,   386,     0,   385,
     0,     0,     0,   630,   666,   548,     0,   699,     0,     0,
     0,    92,    63,   706,   708,     0,   711,     0,   705,   716,
   718,     0,   715,   720,   719,    54,    56,     0,     0,    80,
    78,    98,   102,   571,     0,   477,   446,   476,   486,   486,
   486,   486,     0,   455,     0,   487,   441,   450,   469,   298,
   300,    88,     0,   589,   633,   600,     0,   588,   636,     0,
   127,   127,   642,   638,   635,   614,   613,   611,   612,   595,
   614,   619,   616,   127,   127,     0,   604,   425,   541,   430,
   294,   429,   287,     0,   569,   549,   232,   233,   228,   229,
   234,   235,   230,   231,   127,   127,   567,     0,   550,   552,
   566,     0,     0,     0,   434,   432,   487,   117,   127,   127,
     0,   342,   283,   286,   486,   288,   486,   392,   393,   399,
   487,   395,   401,   487,   294,   294,   413,   410,   294,     0,
     0,   645,   665,   220,   221,   222,   223,   212,   213,   214,
   215,   224,   225,   226,   227,   216,   217,   218,   219,   127,
     0,   654,   650,   652,     0,     0,   668,   550,     0,     0,
     0,     0,     0,   707,    82,    85,   470,     0,   447,   442,
   451,   448,   443,   452,   487,   444,   453,   449,   445,   454,
   456,   463,   464,   303,     0,   305,   614,     0,   633,   585,
     0,     0,     0,     0,   637,     0,     0,   620,   620,   615,
   424,   426,     0,     0,   541,   428,   547,   564,   418,   418,
   543,   544,     0,   568,     0,   435,   436,     0,   124,     0,
   125,     0,   315,   313,   312,   292,   487,     0,   487,   294,
   394,   294,     0,   371,   374,   379,   294,   700,   647,   658,
   418,   659,   655,   656,     0,   473,   631,   462,   471,     0,
    99,     0,   101,   328,    88,     0,     0,   325,     0,   327,
     0,   383,   318,   324,     0,     0,     0,   572,   464,   475,
   276,     0,     0,     0,     0,     0,     0,   529,   606,   606,
   524,   486,     0,   126,   127,   127,     0,     0,   511,   491,
   492,     0,     0,     0,   593,     0,   633,   643,   639,   598,
     0,   623,   617,   621,   618,   427,   542,   352,   268,   294,
   353,   294,   348,   349,   294,   561,   419,   422,   268,   294,
   294,   563,   294,   551,   127,   127,   553,   570,    30,     0,
     0,     0,     0,   289,     0,   486,     0,   294,   486,     0,
   294,   377,   294,   294,   664,     0,   660,   472,   479,   698,
     0,   331,    46,     0,   322,    93,     0,   317,     0,     0,
   330,   321,    83,     0,   527,   514,   515,   516,     0,     0,
     0,   530,     0,   487,   512,     0,     0,   133,   482,   497,
   484,   502,     0,   495,     0,     0,   465,   134,   304,   590,
   601,     0,     0,   625,     0,   294,   425,   541,   559,   294,
   351,   294,   355,   560,   420,   425,   541,   562,   545,   418,
   418,   122,   123,     0,    22,    26,   400,   487,   294,     0,
   403,   402,   294,     0,   406,   662,   663,   657,   418,   100,
     0,   333,     0,     0,   319,   320,     0,     0,   525,   517,
     0,   522,     0,     0,     0,   131,   334,     0,   132,   337,
     0,     0,   464,     0,     0,     0,   481,   486,   480,   501,
     0,   513,   633,   586,   641,   640,   644,   624,     0,   356,
   357,     0,   350,   354,     0,   294,   294,   556,   294,   558,
   314,     0,   405,   294,   408,   294,   661,     0,   326,   323,
     0,   523,     0,   294,   129,     0,   130,     0,     0,     0,
     0,   531,     0,   496,   464,   465,   488,   486,     0,     0,
   622,   358,   546,   554,   555,   557,   404,   407,   332,   526,
   533,     0,   528,   335,   338,     0,     0,   485,   532,   510,
   503,     0,   507,   494,   490,   489,     0,   591,     0,     0,
     0,   534,   535,   518,   486,   486,   483,   498,   531,   509,
   464,   500,     0,     0,   533,     0,     0,   487,   487,   464,
     0,   508,     0,     0,     0,   519,   536,     0,     0,   499,
   504,   537,     0,     0,     0,   336,   339,   531,     0,   539,
     0,   520,     0,     0,     0,     0,   505,   538,   521,   540,
   464,   506,     0,     0,     0
};

static const short yydefgoto[] = {  1163,
     1,     2,     3,    29,    30,    31,    32,   422,   685,   428,
   687,   270,   546,   829,   216,   334,   504,   218,   219,   220,
   221,    33,   222,   223,   489,   488,   486,   837,   487,   224,
   733,   225,   410,   411,   412,   413,   678,   607,    34,   401,
   854,   246,   247,   248,   249,   250,   251,   252,   253,    43,
    44,    45,    46,    47,    48,    49,    50,    51,    52,   665,
   666,   441,   260,   254,    53,   261,    54,    55,    56,    57,
    58,   277,   103,   271,   278,   798,   104,   681,   402,   263,
    60,   382,   383,   384,    61,   796,   902,   831,   832,   833,
  1004,   834,   924,   835,   836,  1016,  1066,  1115,  1019,  1068,
  1116,   684,   280,   913,   883,   884,   281,    62,    63,    64,
    65,   445,   447,   452,   290,    66,   928,   569,   285,   286,
   443,   689,   692,   690,   693,   450,   451,   255,   397,   529,
   915,   887,   888,   532,   533,   272,   608,   609,   610,   611,
   612,   613,   414,   376,   840,  1028,  1032,   510,   511,   512,
   817,   818,   377,   615,   226,   819,   954,   955,  1021,   956,
  1023,   415,   549,  1076,  1029,  1077,  1078,   957,  1075,  1022,
  1130,  1024,  1119,  1148,  1161,  1121,  1101,   860,   861,   943,
  1102,  1111,  1112,  1113,  1151,   650,   774,   667,   893,  1045,
   668,   669,   897,   670,   779,   405,   539,   671,   672,   605,
   227,    68,    89,    69,    70,    71,   519,   867,   390,   757,
  1033,   625,   393,   636,   759,    72,   394,    92,    73,   524,
   641,   525,   646,   873,   874,    74,    75,   189,   456,   726,
   520,   521,   634,   764,  1037,   635,   763,   963,   319,   574,
   723,   724,   725,   916,   917,   458,   576,   320,   321,   322,
   323,   324,   470,   471,   585,   472,   338,   228,   475,   476,
   477,   229,   230,   231
};

static const short yypact[] = {   102,
   110,  3920,  3920,    36,-32768,-32768,-32768,-32768,-32768,   116,
   116,   116,    87,-32768,   193,-32768,   210,-32768,-32768,   210,
   210,-32768,   117,   210,   117,   210,   210,-32768,-32768,-32768,
-32768,-32768,   208,   333,  3085,  1851,  4122,  2017,   514,   425,
   760,  1016,  4150,  4207,  4179,  4233,  1159,  1072,  1293,  1193,
-32768,-32768,   203,-32768,-32768,-32768,-32768,-32768,   116,-32768,
-32768,   315,   317,   334,-32768,-32768,  3920,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   116,   116,
   116,  3414,   228,-32768,-32768,-32768,-32768,-32768,    60,   304,
   187,-32768,-32768,   418,   161,-32768,   363,   210,  3072,-32768,
    89,   116,   389,-32768,  1683,-32768,-32768,-32768,   116,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,   116,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,   116,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,   116,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,   116,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,   116,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,   116,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,   116,-32768,-32768,-32768,-32768,-32768,   400,   333,-32768,
-32768,   225,   264,   225,   276,-32768,   286,-32768,  4312,-32768,
-32768,-32768,-32768,-32768,  3414,  3414,   331,   348,   355,-32768,
   210,-32768,-32768,-32768,  3414,-32768,-32768,  1531,  1905,   361,
   368,   374,-32768,-32768,   435,  3414,   370,   375,-32768,  3481,
  3548,-32768,  4245,  1116,   379,  2142,  3414,-32768,-32768,-32768,
-32768,  1246,-32768,   210,   210,   210,   376,   210,   210,-32768,
-32768,-32768,-32768,   395,   416,  4683,   952,  4696,  1471,   700,
   804,   856,  1082,-32768,   421,   160,   510,   222,   517,-32768,
   333,   333,   116,-32768,   116,-32768,   465,   116,   221,  1501,
   116,-32768,-32768,    89,   116,-32768,   451,-32768,  3723,   259,
   422,   516,  3704,   479,   483,  1256,-32768,   485,-32768,   345,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,   458,  4716,-32768,-32768,-32768,-32768,
  3830,   527,-32768,-32768,-32768,-32768,  3414,  3414,  4716,-32768,
-32768,   502,-32768,   537,   540,-32768,-32768,  4348,  4393,  4716,
   210,-32768,-32768,   542,  3414,  1531,-32768,  1531,-32768,  3414,
  3414,   622,-32768,-32768,  3414,  3414,  3414,  3414,  3414,  3414,
  3414,  3414,  3414,  3414,  3414,  3414,-32768,-32768,   210,   210,
  3414,  3414,   668,   612,-32768,   665,   616,-32768,-32768,-32768,
-32768,    99,-32768,   644,-32768,-32768,   117,   634,-32768,   651,
   635,   643,-32768,   651,-32768,-32768,   359,-32768,   422,   305,
   333,-32768,   719,-32768,-32768,    89,   727,  3146,   650,-32768,
-32768,  1728,    63,-32768,-32768,   693,   400,   400,-32768,   116,
-32768,  1501,   116,   116,-32768,-32768,-32768,  1501,   669,   116,
-32768,-32768,  4683,   952,  4696,  1471,   700,   804,   856,  1082,
-32768,   478,   659,  1040,   225,   116,   225,-32768,   706,   663,
-32768,   345,  4716,-32768,   670,   674,   742,-32768,   527,-32768,
   713,  4774,  4792,   672,-32768,-32768,  3213,  3414,   716,   681,
  4348,-32768,-32768,   731,   698,  4393,-32768,   703,   708,-32768,
  4245,   712,   717,  4245,  4245,  3414,   743,  3414,  3414,  2552,
  1946,  1290,  1257,  1367,  1152,  1152,   452,   452,-32768,-32768,
-32768,-32768,-32768,   718,   375,   709,-32768,-32768,   210,  2232,
   665,-32768,-32768,   724,  1246,  3615,   725,   117,-32768,   744,
  3952,-32768,-32768,   539,  3685,   748,    89,   116,-32768,-32768,
-32768,-32768,   515,-32768,-32768,-32768,   313,   737,  4040,  3414,
  3414,  3280,-32768,   733,-32768,-32768,-32768,-32768,  4068,-32768,
   259,   440,   400,-32768,   780,-32768,-32768,   745,-32768,-32768,
-32768,-32768,-32768,   741,-32768,   746,  3414,   210,   753,   663,
   750,  4429,  1627,-32768,-32768,  4457,  4716,-32768,  4716,  3414,
  4716,-32768,-32768,   375,-32768,  3414,-32768,   716,-32768,-32768,
-32768,   731,-32768,-32768,-32768,   738,   738,   794,  3414,  1818,
  2465,-32768,-32768,-32768,   558,   650,-32768,-32768,    72,    94,
   103,   122,   842,-32768,   758,-32768,-32768,-32768,-32768,-32768,
-32768,   266,   762,-32768,   651,-32768,   615,-32768,-32768,   333,
-32768,-32768,-32768,   250,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,   766,-32768,   359,   359,-32768,
   116,-32768,-32768,   768,-32768,-32768,  4475,  4600,  1052,  1381,
  4487,  4612,  1664,  1401,-32768,-32768,-32768,   770,   559,-32768,
-32768,   404,   764,   769,-32768,-32768,-32768,-32768,   774,   781,
  2659,-32768,-32768,   840,-32768,-32768,-32768,-32768,   785,-32768,
-32768,   786,-32768,-32768,   116,   116,  4245,-32768,   116,   793,
   210,-32768,  3830,  4475,  4600,  4517,  4632,  1052,  1381,  1748,
  1475,  4487,  4612,  4568,  4665,  1664,  1401,  1782,  1826,   796,
   797,-32768,-32768,  4581,  3810,    66,-32768,   800,   799,   801,
  4816,   812,  2412,-32768,-32768,  2377,-32768,   210,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,  2818,-32768,  3414,-32768,-32768,   826,   651,-32768,
   400,   333,  4312,  4011,-32768,   625,  3861,   572,   572,-32768,
-32768,-32768,   817,  4094,-32768,-32768,-32768,-32768,   408,   414,
-32768,-32768,  2921,-32768,   900,-32768,-32768,    66,-32768,   400,
-32768,   333,-32768,-32768,  4245,-32768,-32768,  2659,-32768,   116,
   629,   116,   491,-32768,-32768,-32768,   116,-32768,-32768,-32768,
   408,-32768,-32768,-32768,   816,-32768,-32768,   823,-32768,   210,
-32768,  3414,-32768,-32768,   861,   210,  1905,-32768,   866,  4245,
   828,   829,-32768,-32768,   298,  2590,  3414,-32768,  2982,-32768,
   868,  3414,   874,   837,   841,  3347,   382,   917,   340,   366,
-32768,-32768,   845,-32768,-32768,-32768,   847,   973,-32768,-32768,
-32768,  2908,   413,   671,-32768,   855,   651,-32768,-32768,-32768,
  3414,   886,   852,-32768,   852,-32768,-32768,-32768,    89,   116,
-32768,   116,   523,   534,    95,-32768,-32768,   116,    89,   116,
    95,-32768,   116,-32768,-32768,-32768,-32768,-32768,-32768,   567,
   577,  2412,    66,-32768,    66,-32768,  3414,   232,-32768,  3414,
   292,-32768,   116,    95,-32768,   583,-32768,-32768,-32768,-32768,
  4834,-32768,  4221,  2412,-32768,-32768,  2502,-32768,  2728,  3414,
-32768,-32768,  2377,  4754,-32768,-32768,-32768,-32768,   851,  3414,
   860,-32768,   878,-32768,-32768,   400,   333,-32768,-32768,-32768,
-32768,-32768,   880,   932,  2032,    91,-32768,-32768,-32768,-32768,
-32768,   882,   100,  4245,  3414,   116,   408,   513,-32768,   116,
-32768,   116,-32768,-32768,   116,   414,   414,-32768,-32768,   408,
   414,-32768,-32768,   864,-32768,-32768,-32768,-32768,  4871,  3414,
-32768,-32768,  4871,  3414,-32768,-32768,-32768,-32768,   408,-32768,
  3414,-32768,   869,  2728,-32768,-32768,  4221,  3414,-32768,-32768,
   879,-32768,  3414,   921,   585,-32768,   578,   601,-32768,   822,
   901,   904,-32768,   905,  3414,  2322,-32768,-32768,-32768,-32768,
  3414,-32768,   651,-32768,-32768,-32768,-32768,  4245,   572,   523,
   534,   462,-32768,-32768,  4094,   116,    95,-32768,    95,-32768,
-32768,   629,-32768,  4871,-32768,  4871,-32768,  4730,-32768,-32768,
  4889,-32768,    21,   116,-32768,  1501,-32768,  1501,  3414,  3414,
   956,  2908,   895,-32768,-32768,-32768,-32768,-32768,   896,   915,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
    79,   911,-32768,-32768,-32768,   913,   916,-32768,-32768,-32768,
-32768,   914,-32768,-32768,-32768,-32768,   918,-32768,   934,   210,
    61,   920,-32768,-32768,-32768,-32768,-32768,-32768,  3414,-32768,
-32768,-32768,  3414,   922,    79,   929,    79,-32768,-32768,-32768,
   930,-32768,   923,   995,   113,-32768,-32768,   816,   816,-32768,
-32768,-32768,   949,  1009,   942,-32768,-32768,  3414,  3414,-32768,
   415,-32768,   941,   950,   953,  1025,-32768,-32768,-32768,-32768,
-32768,-32768,  1042,  1043,-32768
};

static const short yypgoto[] = {-32768,
-32768,-32768,-32768,   134,-32768,  -486,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,   -17,-32768,   124,   535,  -330,   417,-32768,
-32768,-32768,   -77,  1084,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,  -411,-32768,   639,-32768,-32768,   -55,   497,  -400,
  -831,    31,    73,    40,   109,   170,   237,   133,   164,  -477,
  -449,  -496,  -489,  -395,  -340,  -480,  -467,  -499,  -482,   507,
   512,  -456,  -232,-32768,  -732,  -244,   940,  1087,  1111,  1423,
-32768,  -754,  -177,  -278,   509,-32768,   673,-32768,   412,    16,
     7,-32768,   555,-32768,  1185,   275,-32768,  -686,-32768,   154,
-32768,  -778,-32768,-32768,   247,-32768,-32768,-32768,-32768,-32768,
-32768,  -140,   242,  -731,   125,  -226,    69,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,   521,  -132,-32768,
   640,-32768,-32768,   192,   191,   652,   538,   431,-32768,-32768,
  -700,  -357,  -389,  -632,-32768,   391,-32768,-32768,-32768,-32768,
-32768,-32768,  -368,-32768,-32768,  -682,    26,-32768,-32768,   594,
  -728,-32768,   291,-32768,-32768,  -730,-32768,-32768,-32768,-32768,
-32768,   809,  -487,    32,  -910,  -416,  -241,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
  -916,   -11,-32768,   -14,-32768,   464,-32768,  -750,-32768,-32768,
   541,   543,-32768,-32768,   454,  -410,-32768,-32768,-32768,-32768,
     3,-32768,   518,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,   -12,   108,  -490,
-32768,   486,-32768,   357,    96,  -465,-32768,-32768,-32768,-32768,
  -382,-32768,-32768,-32768,-32768,   500,-32768,-32768,   373,-32768,
-32768,-32768,   419,-32768,   139,   436,-32768,   568,   574,  -289,
-32768,  -306,-32768,-32768,   561,   677,-32768,-32768,-32768,-32768,
   678,-32768,-32768,-32768
};


#define	YYLAST		4950


static const short yytable[] = {    88,
   423,   282,    90,    91,    67,    67,    94,   531,    88,    98,
   554,   526,    96,   556,   460,   772,   557,    59,    59,   553,
   857,   631,   859,   877,   408,    79,    80,    81,   627,   417,
   958,   459,    35,    35,   629,   900,   407,   279,   632,   530,
   505,    37,    37,   547,   183,   185,   187,   882,   469,   474,
   109,   288,   118,   442,   127,   633,   136,   932,   145,   899,
   154,   661,   163,  -116,   172,   181,   815,  1091,   644,    67,
   858,   661,  -457,   720,    36,    36,   714,   237,   886,   892,
   244,   241,    59,   715,   918,   181,   181,   181,  1109,   662,
   721,  1030,   718,   617,  -458,   712,     9,    35,   661,   662,
  1035,    -1,   105,  -459,  1092,   719,    37,  1125,   859,    -2,
    38,    38,  1071,   233,    59,   181,    59,   325,   326,    15,
    77,    78,  -460,   713,   181,    15,   662,   331,   753,   857,
    93,   256,    93,   181,    41,    41,    76,   584,   343,    36,
   258,   614,   181,   663,  1126,   766,    15,  1110,   234,   378,
  1006,   181,  -116,   663,    82,   816,   858,  -437,  1031,  1144,
   181,   540,   268,   269,  1104,    42,    42,   106,   633,   181,
    17,    39,    39,   257,   985,    38,   986,   716,   181,  -438,
   663,   588,   514,   330,  1036,   505,   592,   515,  -439,   788,
    15,  1015,   740,   743,   746,   749,  1145,    93,   664,    41,
   188,    93,  1131,   801,   722,   217,   803,  -440,   664,   259,
  1132,   442,    84,    85,   381,   984,   386,   387,   388,  1140,
   391,   392,   245,    59,   720,  1060,   406,   714,     9,   115,
    42,  1153,   717,   238,   715,   664,    39,  1003,    40,    40,
  1100,   721,   758,   718,   751,   242,   712,   283,  1046,   234,
  1162,    15,    15,   855,   239,   584,   719,   839,   771,   531,
    83,   109,    15,   118,   631,   127,   864,   136,   618,   181,
   856,   109,   449,   118,   713,    99,   240,   869,   990,  1048,
  1050,   632,   648,   895,    59,    86,   714,   180,   444,    59,
    87,   773,   645,   715,  1083,   232,   661,   454,   284,   256,
   896,    59,   718,    40,   256,   712,    18,    19,   258,   903,
   644,   905,   564,   258,   566,   719,   433,    84,    85,    84,
    85,  -634,    15,   479,   662,   435,   424,   269,   716,   399,
   400,    59,   337,   713,   772,   100,    84,    85,   994,   857,
   267,   257,   929,   772,    59,   448,   257,    84,    85,   754,
   235,   502,   503,   287,   755,    59,   814,    17,   434,   268,
   269,    59,   855,    59,   826,   289,   930,   741,   744,   747,
   750,   236,   268,   269,   517,   291,   866,   259,   663,   856,
   268,   269,   259,   717,    84,    85,   -35,   716,   535,   583,
    86,   101,    86,    17,   436,    87,   460,    87,   327,   906,
   102,   909,   100,   273,   182,   423,   184,  1146,  1147,    86,
   100,   878,   -34,   459,    87,   328,   100,   527,   439,    17,
    86,    59,   329,   186,   863,    87,   528,   269,   339,     6,
     7,     8,   129,   664,   449,   340,   256,    10,    11,    12,
   940,   341,   717,   578,   342,   258,   444,   243,   109,   440,
   118,   234,   127,   344,   136,   437,  1014,    86,   274,    59,
    84,    85,    87,   345,   331,   389,   879,   275,    59,   537,
   373,    17,   889,   264,   433,   880,   269,   265,   257,   395,
   276,   890,   269,   435,   962,   399,   400,   784,   881,   268,
   269,   604,   785,   100,    93,   506,   959,   381,  1155,   396,
  1052,   345,  -580,  1156,   398,   626,  -580,   424,   269,  -257,
   364,   365,   366,   262,   259,   100,   434,   111,     6,     7,
     8,   124,   438,   682,   120,   453,    10,    11,    12,   972,
   269,   544,   403,    86,   645,   419,    59,   910,    87,   420,
    59,    95,    59,    97,    15,  1082,   429,   178,   179,   101,
   449,    35,   436,   739,   742,   433,   748,   256,   102,  1043,
    37,  1044,  -396,  -396,   435,  1039,   258,   661,   430,   657,
   446,   879,   855,   461,   100,   273,   439,   771,   531,   657,
   880,   269,   651,   269,   282,   465,   771,   531,    59,   856,
   970,   269,    59,    36,    59,   662,    59,   434,  -256,   257,
   426,   972,   269,   704,   265,   267,   657,   440,  -286,   598,
   773,   658,   706,   437,   901,   637,   638,   639,   871,   773,
   466,   658,  -286,   467,   640,    93,   480,   872,   872,    38,
   274,   100,   273,   436,   967,   259,   347,   349,   335,   275,
  1138,  1139,   737,   782,   976,   705,   738,   783,   658,   663,
  1080,   982,   276,    41,  1094,   420,  1095,   439,   551,   552,
   908,   983,  -286,   673,   674,   265,  -286,   998,   -84,  1065,
   425,   999,   109,   420,   127,   907,   145,   507,   163,   404,
   438,   707,   416,   808,    42,  1067,   418,   274,   440,   265,
    39,   637,   638,   639,   437,   508,   275,   509,   537,   513,
   760,   637,   638,   639,   664,   710,     8,   124,   659,   276,
   870,   516,    10,    11,    12,    84,    85,   518,   659,   109,
   838,   118,  -632,   127,   522,   136,   523,   145,   538,   154,
    15,   163,   541,   172,   545,   852,   711,   550,   423,    59,
  1041,  1042,   708,   562,   558,   659,   455,   637,   638,   639,
   567,   568,   575,   572,   704,   862,   960,    40,   573,   464,
   581,   438,   586,   706,     6,     7,     8,   133,    59,  1018,
   478,   587,    10,    11,    12,   660,   482,   590,   483,    59,
   577,   591,    59,    35,   378,   660,   594,   534,    86,   599,
    15,   595,    37,    87,    35,   596,   705,   433,    59,   603,
   597,   602,   920,    37,   657,  1017,   435,   620,   922,   709,
     8,   129,   660,   704,   624,   628,    10,    11,    12,   647,
   653,   852,   706,   676,   686,    36,   695,   582,   688,   941,
   537,   696,   707,   701,   555,   404,    36,    96,   699,   434,
   735,   559,   409,   752,  -258,   756,   658,   885,   891,   267,
   770,   777,  -286,   781,   786,   705,   710,   565,   789,   787,
   537,    38,     8,   133,   862,   791,  -286,   267,    10,    11,
    12,   911,    38,   800,   802,   436,   853,    59,   807,   914,
   810,   812,   820,   571,   821,    41,    15,   711,   783,   268,
   269,   707,    35,   708,    59,   823,    41,   865,   872,   439,
   876,    37,   898,   975,    59,   816,  -286,   -32,   375,   256,
  -286,   908,   925,   926,   -33,   710,    42,   927,   258,   256,
   935,   936,    39,   652,   942,   937,   961,    42,   258,   945,
   440,   948,   965,    39,    36,  1010,   437,   560,   561,   649,
   966,   425,   425,   659,  1012,  1013,   711,  1025,  1026,  1051,
   337,   257,   708,  1034,  1059,     5,    93,    93,     8,   111,
   709,   257,   853,  1062,    10,    11,    12,  1064,  1069,   939,
    38,  1070,  1072,  1098,   107,   112,   116,   121,  1103,  1107,
    14,   181,   143,   148,   152,   157,  1108,   259,   949,    40,
   950,   951,   952,   953,    41,  1114,  1117,   259,  1120,  1118,
    40,  1123,  1122,   438,  1143,    17,  1142,   729,  1127,   730,
   660,   732,  1134,  1136,  1141,  1020,  1149,   630,  1150,   709,
     6,     7,     8,   138,  1157,    42,  1152,    23,    10,    11,
    12,    39,    25,  1158,  1160,   399,   400,  1159,   534,   534,
   431,  1164,  1165,     5,   399,   400,     8,     9,  1047,  1049,
   623,   548,    10,    11,    12,   679,     6,     7,     8,   124,
   680,   683,   775,  1011,    10,    11,    12,   914,    14,   621,
    15,    16,   904,   536,   862,   657,     6,     7,     8,   165,
  1005,   931,    15,   563,    10,    11,    12,    59,     8,   138,
   700,  1040,  1124,    17,    10,    11,    12,   987,    40,   992,
  -259,  1105,    35,   570,   619,   698,   804,   805,   919,  1106,
   806,    37,  1137,  1135,   776,    23,   727,   658,   728,   780,
    25,   108,   113,   117,   122,   875,   767,   761,   762,   144,
   149,   153,   158,   765,  1081,   868,  1063,  1057,   809,   702,
   768,   769,   813,     0,    36,   703,   734,   589,  1073,   125,
   130,   134,   139,   593,  1079,     0,  -265,   161,   166,   170,
   175,   778,   778,     6,     7,     8,   160,     0,     0,   534,
   534,    10,    11,    12,     0,   790,   792,     0,   367,   368,
    38,   369,   370,   371,   372,   107,   112,   116,   121,    15,
     0,     0,  1096,  1097,     0,  1099,     0,     6,     7,     8,
   174,   534,     0,     0,    41,    10,    11,    12,   362,   363,
   364,   365,   366,     0,   659,     0,   811,     0,   912,   110,
   114,   119,   123,   128,   132,   137,   141,   146,   150,   155,
   159,   164,   168,   173,   177,    42,     0,     0,     0,     0,
     0,    39,  1099,  -264,     0,     0,  1133,     0,    84,    85,
     6,     7,   379,   380,     0,     0,   431,     0,     0,     5,
   630,     0,     8,     9,     0,     0,     0,     0,    10,    11,
    12,  1099,  1154,   971,   973,     0,     0,  -267,     0,     0,
     0,   660,     0,     0,    14,     0,    15,    16,     0,     0,
     0,   968,     0,   969,     0,     0,   974,     6,     7,     8,
   169,   977,   978,     0,   979,    10,    11,    12,    40,    17,
   359,   360,   361,   362,   363,   364,   365,   366,   616,   991,
     0,    86,   995,    15,   996,   997,    87,     0,     0,     0,
     0,    23,   108,   113,   117,   122,    25,     0,     0,     0,
   432,  -387,   358,   359,   360,   361,   362,   363,   364,   365,
   366,   946,   947,     0,   677,     0,     0,   534,   534,     0,
   125,   130,   134,   139,     0,     0,   534,   534,   691,   694,
   534,   534,   107,   112,   116,   121,     0,  -266,     0,     0,
     0,   404,     0,   404,     0,     6,     7,     8,   129,   534,
     0,   980,   981,    10,    11,    12,     0,     0,     0,     0,
  1053,     0,     0,     0,  1055,     6,     7,     8,   165,     0,
   462,   463,     0,    10,    11,    12,   385,   616,   616,   745,
   616,   360,   361,   362,   363,   364,   365,   366,   481,     0,
   971,   973,   973,   484,   485,     0,     0,     0,   490,   491,
   492,   493,   494,   495,   496,   497,   498,   499,   500,   501,
     0,     0,     0,     0,     0,     0,     0,  1084,  1085,     0,
  1086,   126,   131,   135,   140,  1087,     0,  1088,     0,   162,
   167,   171,   176,     0,     5,  1093,     0,     8,   120,     6,
     7,     8,   138,    10,    11,    12,     0,    10,    11,    12,
     0,     0,     0,   797,     0,   799,     0,     0,     0,    14,
     0,   409,     0,     0,  -486,  -486,  -486,  -486,  -486,     0,
     0,     0,     0,  -486,  -486,  -486,     0,     0,     0,   108,
   113,   117,   122,     0,    17,     0,     0,     0,     0,  -486,
     0,   332,     0,   190,     5,     0,     0,     8,     9,   191,
   192,     0,   193,    10,    11,    12,    23,   125,   130,   134,
   139,    25,     0,     0,  -486,     0,     0,     0,     0,    14,
   194,    15,    16,     0,   195,   196,   197,   198,   199,     0,
     0,   600,   601,   200,     0,     0,  -486,     0,     0,   201,
     0,  -486,   202,     0,    17,     0,     0,   203,   204,   205,
  -115,     0,     0,   206,   207,     0,   107,   112,   208,   209,
   143,   148,     0,   210,     0,   211,    23,     0,     0,     0,
   212,    25,     0,     0,     0,     0,     0,   213,   214,     0,
   333,     0,   215,     0,     0,     0,     0,   409,     0,     0,
     5,     6,     7,     8,     9,     0,     0,     0,     0,    10,
    11,    12,     0,   107,   112,   116,   121,     0,     0,     0,
   697,   143,   148,   152,   157,    14,     0,    15,     0,     0,
   944,     0,     0,   731,     0,     0,     0,     0,     6,     7,
     8,   160,   126,   131,   135,   140,    10,    11,    12,     0,
    17,     0,   736,   266,     0,     0,   -28,   -28,   -28,   -28,
   -28,     0,     0,     0,    15,   -28,   -28,   -28,     0,   385,
     0,     0,    23,     0,     0,     0,     0,    25,     0,     0,
   267,   -28,     0,  -286,   988,     0,  -651,   694,     0,     0,
     0,     0,     0,     0,     0,     0,     0,  -286,   409,     0,
     0,  -118,  -118,  -118,  -118,  -118,   -28,     0,     0,     0,
  -118,  -118,  -118,   108,   113,     0,     0,   144,   149,     0,
   268,   269,     6,     7,     8,   133,  -118,     0,   -28,     0,
    10,    11,    12,   -28,   795,     0,     0,  -286,     0,   125,
   130,  -286,   -28,   161,   166,     0,     0,     0,    15,     0,
     0,  -118,     0,     0,     0,     0,     6,     7,     8,   169,
   108,   113,   117,   122,    10,    11,    12,     0,   144,   149,
   153,   158,     0,  -118,     0,     0,     0,     0,  -118,     0,
     0,     0,    15,     0,     0,     0,   830,  -118,   125,   130,
   134,   139,     0,     0,     0,     0,   161,   166,   170,   175,
     6,     7,     8,   174,     0,     0,   745,     0,    10,    11,
    12,   110,   114,   128,   132,   146,   150,   164,   168,     0,
     0,     0,     0,     0,     5,     6,     7,     8,   111,   126,
   131,   135,   140,    10,    11,    12,   354,   355,   356,   357,
   358,   359,   360,   361,   362,   363,   364,   365,   366,    14,
     0,   795,     0,     0,     0,     0,   745,     0,   110,   114,
   119,   123,   128,   132,   137,   141,   146,   150,   155,   159,
   164,   168,   173,   177,    17,   921,     0,   190,     0,     0,
   923,     0,     0,   191,   192,     0,   193,     0,     0,   830,
   933,     0,     0,  1128,  1129,   934,    23,     0,     0,     0,
     0,    25,     0,     0,   194,  -253,    16,     0,   195,   196,
   197,   198,   199,     0,     0,     0,     0,   200,     0,     0,
     0,     0,     0,   201,   964,     0,   202,     0,     0,     0,
     0,   203,   204,   205,     0,     0,     0,   206,   207,     0,
     0,     0,   208,   209,     0,     0,     0,   210,     0,   211,
   336,     0,     0,     0,   212,   830,     0,     0,     0,     0,
   989,   213,   214,   993,     0,     0,   215,   357,   358,   359,
   360,   361,   362,   363,   364,   365,   366,   830,     0,     0,
   830,     0,   830,  1007,     0,     0,     0,     0,     0,     0,
     5,     6,     7,     8,   120,     0,     0,     0,     0,    10,
    11,    12,  1027,     0,  -464,  -464,     0,     0,     0,     0,
  -464,  -464,     0,  -464,     0,    14,     0,  -464,  1038,  -464,
  -464,  -464,  -464,  -464,  -464,  -464,  -464,  -464,  -464,  -464,
     0,  -464,     0,  -464,     0,  -464,  -464,  -464,  -464,  -464,
    17,     0,     0,  1054,  -464,     0,     0,  1056,     0,     0,
  -464,   126,   131,  -464,  1058,   162,   167,   830,  -464,  -464,
  -464,  1061,    23,     0,  -464,  -464,     0,    25,     0,  -464,
  -464,  -255,     0,     0,  -464,     0,  -464,  -464,     0,     0,
     0,  -464,  -464,     0,     0,     0,  -464,     0,  -464,  -464,
     0,  -464,     0,  -464,     0,     0,     0,     0,     0,     0,
   126,   131,   135,   140,     0,     0,     0,     0,   162,   167,
   171,   176,   374,     0,  -462,  -462,  -462,  -462,  -462,  -462,
  -462,  -462,     0,  -462,  -462,  -462,  -462,  -462,     0,  -462,
  -462,  -462,  -462,  -462,  -462,  -462,  -462,  -462,  -462,  -462,
  -462,  -462,  -462,  -462,  -462,  -462,  -462,  -462,  -462,  -462,
     0,     0,     0,     0,  -462,     0,     0,     0,     0,     0,
  -462,     0,     0,  -462,     0,  -462,     0,     0,  -462,  -462,
  -462,     0,     0,     0,  -462,  -462,     0,     0,     0,  -462,
  -462,     0,     0,     0,  -462,     0,  -462,  -462,     0,     0,
     0,  -462,  -462,     0,     0,     0,  -462,   375,  -462,  -462,
     0,  -462,   606,  -462,  -486,  -486,  -486,  -486,  -486,  -486,
  -486,  -486,     0,  -486,  -486,  -486,  -486,  -486,     0,  -486,
  -486,  -486,  -486,  -486,  -486,  -486,  -486,  -486,  -486,  -486,
  -486,  -486,  -486,  -486,     0,  -486,  -486,  -486,  -486,  -486,
     0,     0,     0,     0,  -486,     0,     0,     0,     0,     0,
  -486,     0,     0,  -486,     0,  -486,     0,     0,  -486,  -486,
  -486,     0,     0,     0,  -486,  -486,     0,     0,     0,  -486,
  -486,     0,     0,     0,  -486,     0,  -486,  -486,     0,     0,
     0,  -486,  -486,     0,     0,     0,  -486,     0,  -486,  -486,
     0,  -486,  1074,  -486,  -493,  -493,     0,     0,     0,     0,
  -493,  -493,     0,  -493,     0,     0,     0,  -493,     0,  -493,
  -493,  -493,  -493,  -493,  -493,  -493,  -493,  -493,  -493,  -493,
     0,  -493,     0,  -493,     0,  -493,  -493,  -493,  -493,  -493,
     0,     0,     0,     0,  -493,     0,     0,     0,     0,     0,
  -493,     0,     0,  -493,     0,     0,     0,     0,  -493,  -493,
  -493,     0,     0,     0,  -493,  -493,     0,     0,     0,  -493,
  -493,     0,     0,     0,  -493,     0,  -493,  -493,     0,     0,
     0,  -493,  -493,     0,     0,     0,  -493,     0,  -493,  -493,
     0,  -493,   824,  -493,   825,    85,     0,     0,     0,     0,
   191,   192,   352,   193,   353,   354,   355,   356,   357,   358,
   359,   360,   361,   362,   363,   364,   365,   366,     0,     0,
     0,   194,     0,    16,     0,   195,   196,   197,   198,   199,
     0,     0,     0,     0,   200,     0,     0,     0,     0,     0,
   201,     0,     0,   202,     0,     0,     0,     0,   203,   204,
   205,     0,     0,     0,   206,   207,     0,     0,   826,   208,
   827,     0,     0,     0,   210,     0,   211,    86,     0,     0,
     0,   212,    87,     0,     0,     0,     0,  -316,   213,   214,
     0,   828,   824,   215,   825,    85,     0,     0,     0,     0,
   191,   192,     0,   193,   355,   356,   357,   358,   359,   360,
   361,   362,   363,   364,   365,   366,     0,     0,     0,     0,
     0,   194,     0,    16,     0,   195,   196,   197,   198,   199,
     0,     0,     0,     0,   200,     0,     0,     0,     0,     0,
   201,     0,     0,   202,     0,     0,     0,     0,   203,   204,
   205,     0,     0,     0,   206,   207,     0,     0,   826,   208,
   827,     0,     0,     0,   210,     0,   211,    86,     0,     0,
     0,   212,    87,     0,     0,     0,     0,  -384,   213,   214,
   824,   828,   190,   215,     0,     0,     0,     0,   191,   192,
     0,   193,   356,   357,   358,   359,   360,   361,   362,   363,
   364,   365,   366,     0,     0,     0,     0,     0,     0,   194,
     0,    16,     0,   195,   196,   197,   198,   199,     0,     0,
     0,     0,   200,     0,  -329,     0,     0,     0,   201,     0,
     0,   202,     0,     0,     0,     0,   203,   204,   205,     0,
     0,     0,   206,   207,     0,     0,  -329,   208,   209,   793,
     0,   190,   210,     0,   211,     0,     0,   191,   192,   212,
   193,     0,     0,     0,     0,     0,   213,   214,     0,   828,
     0,   215,     0,     0,     0,     0,     0,     0,   194,     0,
    16,     0,   195,   196,   197,   198,   199,     0,     0,     0,
     0,   200,     0,     0,     0,     0,     0,   201,     0,     0,
   202,     0,     0,     0,     0,   203,   204,   205,     0,     0,
     0,   206,   207,     0,     0,     0,   208,   209,   824,     0,
   190,   210,     0,   211,     0,     0,   191,   192,   212,   193,
     0,     0,     0,     0,     0,   213,   214,     0,   794,     0,
   215,     0,     0,     0,     0,     0,     0,   194,     0,    16,
     0,   195,   196,   197,   198,   199,     0,     0,     0,     0,
   200,     0,     0,     0,     0,     0,   201,     0,     0,   202,
     0,     0,     0,     0,   203,   204,   205,     0,     0,     0,
   206,   207,     0,     0,     0,   208,   209,     0,     0,     0,
   210,     0,   211,     0,     0,     0,     0,   212,     0,     0,
     0,     0,     0,     0,   213,   214,     0,   828,     0,   215,
   825,   841,     6,     7,     8,     9,   191,   192,     0,   193,
    10,    11,    12,     0,     0,     0,     0,     0,     0,   842,
   843,   844,   845,   846,   847,   848,    14,   194,    15,    16,
     0,   195,   196,   197,   198,   199,     0,     0,     0,     0,
   200,     0,     0,     0,     0,     0,   201,     0,     0,   202,
     0,    17,     0,     0,   203,   204,   205,     0,     0,     0,
   206,   207,     0,     0,     0,   208,   209,     0,     0,     0,
   210,     0,   211,   849,     0,     0,     0,   212,   850,     0,
     0,     0,   851,     0,   213,   214,     0,   816,     0,   215,
   190,     5,     6,     7,     8,     9,   191,   192,     0,   193,
    10,    11,    12,     0,     5,     6,     7,     8,     9,     0,
     0,   894,     0,    10,    11,    12,    14,   194,    15,    16,
     0,   195,   196,   197,   198,   199,     0,     0,     0,    14,
   200,    15,     0,     0,     0,     0,   201,     0,     0,   202,
     0,    17,     0,     0,   203,   204,   205,     0,     0,     0,
   206,   207,     0,     0,    17,   208,   209,     0,     0,     0,
   210,     0,   211,    23,   825,    85,     0,   212,    25,     0,
   191,   192,     0,   193,   213,   214,    23,     0,     0,   215,
     0,    25,     0,   842,   843,   844,   845,   846,   847,   848,
     0,   194,     0,    16,     0,   195,   196,   197,   198,   199,
     0,     0,     0,     0,   200,     0,     0,     0,     0,     0,
   201,     0,     0,   202,     0,     0,     0,     0,   203,   204,
   205,     0,     0,     0,   206,   207,     0,     0,     0,   208,
   209,     0,     0,     0,   210,     0,   211,    86,     0,     0,
     0,   212,    87,     0,     0,     0,   851,     0,   213,   214,
     0,   816,     0,   215,   190,     5,     0,     0,     8,     9,
   191,   192,     0,   193,    10,    11,    12,     0,     5,     6,
     7,     8,   106,     0,     0,     0,     0,    10,    11,    12,
    14,   194,    15,    16,     0,   195,   196,   197,   198,   199,
     0,     0,     0,    14,   200,    15,     0,     0,     0,     0,
   201,     0,     0,   202,     0,    17,     0,     0,   203,   204,
   205,     0,     0,     0,   206,   207,     0,     0,    17,   208,
   209,     0,     0,     0,   210,     0,   211,    23,   190,     0,
     0,   212,    25,     0,   191,   192,     0,   193,   213,   214,
    23,     0,     0,   215,     0,    25,     0,     0,     0,  -252,
     0,     0,     0,     0,     0,   194,     0,    16,     0,   195,
   196,   197,   198,   199,     0,     0,     0,     0,   200,     0,
     0,     0,     0,     0,   201,     0,     0,   202,     0,     0,
     0,     0,   203,   204,   542,     0,     0,     0,   206,   207,
     0,     0,     0,   208,   209,   190,     0,     0,   210,     0,
   211,   191,   192,     0,   193,   212,     0,     0,     0,     0,
     0,     0,   213,   214,     0,     0,   543,   215,     0,     0,
     0,     0,   194,     0,    16,     0,   195,   196,   197,   198,
   199,     0,     0,     0,     0,   200,     0,     0,     0,     0,
     0,   201,     0,     0,   202,     0,     0,     0,     0,   203,
   204,   205,     0,     0,     0,   206,   207,     0,     0,     0,
   208,   209,   190,     0,     0,   210,     0,   211,   191,   192,
     0,   193,   212,     0,     0,     0,     0,     0,     0,   213,
   214,     0,   582,     0,   215,     0,     0,     0,     0,   194,
     0,    16,     0,   195,   196,   197,   198,   199,     0,     0,
     0,     0,   200,     0,     0,     0,     0,     0,   201,     0,
     0,   202,     0,     0,     0,     0,   203,   204,   205,     0,
     0,     0,   206,   207,     0,     0,     0,   208,   209,   190,
     0,     0,   210,     0,   211,   191,   192,     0,   193,   212,
     0,     0,     0,     0,     0,     0,   213,   214,     0,     0,
   675,   215,     0,     0,     0,     0,   194,     0,    16,     0,
   195,   196,   197,   198,   199,     0,     0,     0,     0,   200,
     0,     0,     0,     0,     0,   201,     0,     0,   202,     0,
     0,     0,     0,   203,   204,   205,     0,     0,     0,   206,
   207,     0,     0,     0,   208,   209,   190,     0,     0,   210,
     0,   211,   191,   192,     0,   193,   212,     0,     0,     0,
     0,   938,     0,   213,   214,     0,     0,     0,   215,     0,
     0,     0,     0,   194,     0,    16,     0,   195,   196,   197,
   198,   199,     0,     0,     0,     0,   200,     0,     0,     0,
     0,     0,   201,     0,     0,   202,     0,     0,     0,     0,
   203,   204,   205,     0,     0,     0,   206,   207,     0,     0,
     0,   208,   209,   190,     0,     0,   210,     0,   211,   191,
   192,     0,   193,   212,     0,     0,     0,     0,     0,     0,
   213,   214,     0,     0,     0,   215,     0,     0,     0,     0,
   194,     0,    16,     0,   195,   196,   197,   198,   199,     0,
     0,     0,     0,   200,     0,     0,     0,     0,     0,   201,
     0,     0,   202,     0,     0,     0,     0,   203,   204,   205,
     0,     0,     0,   206,   207,     0,     0,     0,   346,   209,
   190,     0,     0,   210,     0,   211,   191,   192,     0,   193,
   212,     0,     0,     0,     0,     0,     0,   213,   214,     0,
     0,     0,   215,     0,     0,     0,     0,   194,     0,    16,
     0,   195,   196,   197,   198,   199,     0,     0,     0,     0,
   200,     0,     0,     0,     0,     0,   201,     0,     0,   202,
     0,     0,     0,     0,   203,   204,   205,     0,     0,     0,
   206,   207,     0,     0,     0,   348,   209,   622,     0,     0,
   210,     0,   211,   191,   192,     0,   193,   212,     0,     0,
     0,     0,     0,     0,   213,   214,     0,     0,     0,   215,
     0,     0,     0,     0,   194,     0,    16,     0,   195,   196,
   197,   198,   199,     0,     0,     0,     0,   200,     0,     0,
     0,     0,     0,   201,     0,     0,   202,     0,     0,     0,
     0,   203,   204,   205,     0,     0,     0,   206,   207,     0,
     0,     0,   208,   209,     0,   642,     0,   210,     5,   211,
     0,     8,     9,     0,   212,     0,     0,    10,    11,    12,
     0,   213,   214,     0,   427,     0,   215,   -24,   -24,   -24,
   -24,   -24,     0,    14,     0,    15,   -24,   -24,   -24,     0,
     0,     0,     0,   421,     0,     0,   -20,   -20,   -20,   -20,
   -20,   267,   -24,     0,  -286,   -20,   -20,   -20,    17,     0,
     0,     0,     0,     0,     0,     0,     0,     0,  -286,     0,
   267,   -20,     0,  -286,     0,     0,     0,   -24,     0,     0,
    23,  -610,  -610,  -610,     0,    25,     0,  -286,     0,   643,
  -610,   268,   269,     0,     0,     0,   -20,     0,     0,   -24,
     0,     0,     0,     0,   -24,     0,     0,     0,  -286,     0,
     0,     0,  -286,   -24,     0,     0,     0,     0,   -20,     0,
     0,     0,     0,   -20,     0,     0,     0,  -286,     0,     0,
   409,  -286,   -20,  -653,  -653,  -653,  -653,  -653,     0,     0,
     0,     0,  -653,  -653,  -653,     0,     0,     0,     0,     0,
     0,     0,   292,   293,     0,     0,   294,   295,  -653,     0,
  -653,   296,   297,   298,   299,   300,   301,   302,   303,   304,
   305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     0,   642,     0,  -653,     5,     0,     0,     8,     9,     0,
     0,     0,     0,    10,    11,    12,   315,     0,     0,     0,
     0,     0,     0,     0,     0,  -653,     0,     0,     0,    14,
  -653,    15,     0,     0,     0,     0,     0,     0,     0,  -653,
     0,     0,     0,     0,     0,   317,     0,     0,     0,     0,
   318,     0,     0,     0,    17,     0,     0,     0,   457,     0,
     4,     0,  -127,     5,     6,     7,     8,     9,     0,     0,
     0,     0,    10,    11,    12,     0,    23,  -609,  -609,  -609,
     0,    25,     0,     0,     0,   643,  -609,    13,    14,     0,
    15,    16,     4,     0,  -127,     5,     6,     7,     8,     9,
     0,     0,     0,     0,    10,    11,    12,     0,     0,     0,
     0,     0,     0,    17,     0,     0,    18,    19,  -127,     0,
    14,     0,    15,     0,     0,     0,     0,  -127,     0,    20,
    21,    22,     0,     0,     0,    23,     0,     0,     0,    24,
    25,    26,    27,     0,    28,    17,     0,     0,    18,    19,
  -127,     4,     0,  -127,     5,     6,     7,     8,     9,  -127,
     0,     0,     0,    10,    11,    12,     0,    23,     0,     0,
     0,     0,    25,     0,     0,     0,    28,     0,     0,    14,
   654,    15,   655,     5,     6,     7,     8,     9,     0,     0,
   656,     0,    10,    11,    12,     0,     0,     0,     0,     0,
     0,     0,     0,     0,    17,     0,     0,     0,    14,  -127,
     0,     5,     6,     7,     8,     9,     0,     0,  -127,     0,
    10,    11,    12,     0,     0,     0,    23,     0,     0,     0,
     0,    25,     0,    17,   654,    28,    14,     5,     6,     7,
     8,     9,     0,     0,   656,     0,    10,    11,    12,     0,
     0,     0,     0,     0,     0,    23,     0,     0,     0,     0,
    25,    17,    14,  -548,     0,     5,     6,     7,     8,   115,
     0,     0,     0,     0,    10,    11,    12,     0,     0,     0,
     0,     0,     0,    23,     0,     0,     0,    17,    25,     0,
    14,     0,    15,     5,     6,     7,     8,   142,     0,     0,
     0,     0,    10,    11,    12,     0,     0,     0,     0,    23,
     0,     0,     0,     0,    25,    17,     0,  -548,    14,     0,
    15,     0,     5,     6,     7,     8,   151,     0,     0,     0,
     0,    10,    11,    12,     0,     0,     0,    23,     0,     0,
     0,     0,    25,    17,     0,     0,  -254,    14,     0,    15,
     5,     6,     7,     8,   147,     0,     0,     0,     0,    10,
    11,    12,     0,     0,     0,    23,     0,     0,     0,     0,
    25,  1001,    17,     0,  -260,    14,     5,     6,     7,     8,
   156,     0,     0,     0,     0,    10,    11,    12,     0,     0,
     0,     0,     0,     0,    23,     0,     0,     0,     0,    25,
    17,    14,     0,  -262,   350,   351,   352,     0,   353,   354,
   355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
   365,   366,    23,     0,     0,     0,    17,    25,   350,   351,
   352,  -261,   353,   354,   355,   356,   357,   358,   359,   360,
   361,   362,   363,   364,   365,   366,     0,     0,    23,     0,
     0,  1002,     0,    25,   292,   293,     0,  -263,   294,   295,
     0,     0,     0,   296,   297,   298,   299,   300,   301,   302,
   303,   304,   305,   306,   307,   308,   309,   310,   311,   312,
   313,   314,     0,     0,     0,     0,     0,     0,     0,     0,
   292,   293,     0,     0,   294,   295,     0,     0,   315,   296,
   297,   298,   299,   300,   301,   302,   303,   304,   305,   306,
   307,   308,   309,   310,   311,   312,   313,   314,     0,   316,
     0,     0,     0,     0,     0,     0,     0,   317,     0,     0,
     0,     0,   318,     0,   468,   292,   293,     0,     0,   294,
   295,     0,     0,     0,   296,   297,   298,   299,   300,   301,
   302,   303,   304,   305,   306,   307,   308,   309,   310,   311,
   312,   313,   314,   317,     0,     0,     0,     0,   318,     0,
     0,   292,   293,     0,     0,   294,   295,     0,     0,   473,
   296,   297,   298,   299,   300,   301,   302,   303,   304,   305,
   306,   307,   308,   309,   310,   311,   312,   313,   314,     0,
     5,     6,     7,     8,     9,     0,     0,   656,   317,    10,
    11,    12,     0,   318,     0,   315,     0,     0,     5,     6,
     7,     8,   106,     0,     0,    14,     0,    10,    11,    12,
     5,     6,     7,     8,   142,     0,     0,     0,     0,    10,
    11,    12,     0,    14,   317,    15,     0,     0,     0,   318,
    17,     0,     0,     0,     0,    14,     0,    15,     0,     0,
     5,     6,     7,     8,   115,     0,     0,     0,    17,    10,
    11,    12,    23,     0,     0,     0,     0,    25,     0,     0,
    17,     0,     0,     0,     0,    14,     0,    15,     0,     0,
    23,     0,     0,     0,     0,    25,     0,     0,     0,     0,
     0,     0,    23,     0,     0,     0,     0,    25,     0,     0,
    17,     5,     6,     7,     8,   151,     0,     0,     0,     0,
    10,    11,    12,     0,     5,     6,     7,     8,     9,     0,
     0,     0,    23,    10,    11,    12,    14,    25,    15,     0,
     0,     0,     0,     5,     6,     7,     8,   111,     0,    14,
     0,    15,    10,    11,    12,     5,     6,     7,     8,   147,
     0,    17,     0,     0,    10,    11,    12,     0,    14,     0,
     0,     0,     0,     0,    17,     5,     6,     7,     8,   120,
    14,     0,     0,    23,    10,    11,    12,     0,    25,     0,
     0,     0,     0,    17,     0,     0,    23,     0,     0,     0,
    14,    25,     0,     0,     0,    17,     0,     0,     5,     6,
     7,     8,   156,     0,     0,    23,     0,    10,    11,    12,
    25,     0,     0,     0,     0,    17,     5,    23,     0,     8,
   106,     0,    25,    14,     0,    10,    11,    12,     0,     5,
     0,     0,     8,   115,     0,     0,     0,    23,    10,    11,
    12,    14,    25,    15,     0,     0,     0,     0,    17,     5,
     0,     0,     8,     9,    14,     0,    15,     0,    10,    11,
    12,     0,     0,     0,     0,     0,    17,     0,     0,     0,
    23,     0,     0,     0,    14,    25,    15,     0,     0,    17,
     0,     0,     0,     0,     0,     0,     0,     0,    23,     0,
     0,     0,     0,    25,  1008,     0,     0,     0,     0,    17,
     0,    23,     0,   350,   351,   352,    25,   353,   354,   355,
   356,   357,   358,   359,   360,   361,   362,   363,   364,   365,
   366,    23,     0,     0,     0,     0,    25,   350,   351,   352,
  1009,   353,   354,   355,   356,   357,   358,   359,   360,   361,
   362,   363,   364,   365,   366,     0,     0,   350,   351,   352,
  1089,   353,   354,   355,   356,   357,   358,   359,   360,   361,
   362,   363,   364,   365,   366,   350,   351,   352,     0,   353,
   354,   355,   356,   357,   358,   359,   360,   361,   362,   363,
   364,   365,   366,     0,     0,     0,     0,     0,     0,   350,
   351,   352,   579,   353,   354,   355,   356,   357,   358,   359,
   360,   361,   362,   363,   364,   365,   366,   350,   351,   352,
   580,   353,   354,   355,   356,   357,   358,   359,   360,   361,
   362,   363,   364,   365,   366,     0,     0,     0,     0,     0,
     0,    15,     0,     0,   822,     0,     0,     0,     0,     0,
     0,     0,     0,     0,   350,   351,   352,  1000,   353,   354,
   355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
   365,   366,   350,   351,   352,  1090,   353,   354,   355,   356,
   357,   358,   359,   360,   361,   362,   363,   364,   365,   366
};

static const short yycheck[] = {    17,
   279,   179,    20,    21,     2,     3,    24,   397,    26,    27,
   422,   394,    25,   424,   321,   648,   428,     2,     3,   420,
   753,   521,   753,   774,   269,    10,    11,    12,   519,   274,
   862,   321,     2,     3,   521,   790,   269,   178,   521,   397,
   371,     2,     3,   412,    62,    63,    64,   779,   338,   339,
    35,   184,    37,   286,    39,   521,    41,   836,    43,   788,
    45,   539,    47,     1,    49,    59,     1,    47,   525,    67,
   753,   549,     1,   573,     2,     3,   573,    90,   779,   780,
    98,    94,    67,   573,   815,    79,    80,    81,    10,   539,
   573,     1,   573,   510,     1,   573,     8,    67,   576,   549,
     1,     0,    34,     1,    84,   573,    67,    47,   839,     0,
     2,     3,  1023,    54,    99,   109,   101,   195,   196,    31,
    85,    86,     1,   573,   118,    31,   576,   205,   616,   862,
    23,   101,    25,   127,     2,     3,     3,   468,   216,    67,
   101,   510,   136,   539,    84,   636,    31,    69,    89,   227,
   929,   145,    90,   549,    68,    90,   839,    86,    68,    47,
   154,   406,    68,    69,  1075,     2,     3,     8,   634,   163,
    54,     2,     3,   101,   903,    67,   905,   573,   172,    86,
   576,   471,    84,   201,    85,   516,   476,    89,    86,   677,
    31,   946,   609,   610,   611,   612,    84,    90,   539,    67,
    67,    94,  1119,   691,   573,    82,   694,    86,   549,   101,
  1121,   444,     3,     4,   232,   902,   234,   235,   236,  1130,
   238,   239,    99,   208,   724,  1004,     6,   724,     8,     8,
    67,  1148,   573,    47,   724,   576,    67,   924,     2,     3,
  1072,   724,   625,   724,   613,    85,   724,   179,   980,    89,
  1161,    31,    31,   753,    68,   586,   724,   745,   648,   649,
    68,   246,    31,   248,   764,   250,   757,   252,   510,   263,
   753,   256,   290,   258,   724,    68,    90,   764,    47,   980,
   981,   764,   527,   783,   269,    76,   783,    85,   286,   274,
    81,   649,   525,   783,  1045,    68,   774,   315,    74,   269,
   783,   286,   783,    67,   274,   783,    57,    58,   269,   797,
   767,   799,   445,   274,   447,   783,   286,     3,     4,     3,
     4,    72,    31,   341,   774,   286,    68,    69,   724,   261,
   262,   316,   209,   783,   967,     3,     3,     4,    47,  1072,
    28,   269,    45,   976,   329,     1,   274,     3,     4,    84,
    47,   369,   370,    90,    89,   340,   725,    54,   286,    68,
    69,   346,   862,   348,    67,    90,    69,   609,   610,   611,
   612,    68,    68,    69,   387,    90,   759,   269,   774,   862,
    68,    69,   274,   724,     3,     4,    47,   783,    84,   467,
    76,    59,    76,    54,   286,    81,   703,    81,    68,   800,
    68,   802,     3,     4,    90,   684,    90,  1138,  1139,    76,
     3,     4,    47,   703,    81,    68,     3,    59,   286,    54,
    76,   406,    68,    90,   755,    81,    68,    69,    68,     5,
     6,     7,     8,   774,   452,    68,   406,    13,    14,    15,
    59,    68,   783,   461,    10,   406,   444,    85,   433,   286,
   435,    89,   437,    84,   439,   286,   944,    76,    59,   444,
     3,     4,    81,    89,   542,    90,    59,    68,   453,   401,
    92,    54,    59,    85,   444,    68,    69,    89,   406,    85,
    81,    68,    69,   444,   867,   417,   418,    84,    81,    68,
    69,   509,    89,     3,   387,   372,    84,   515,    84,    84,
   988,    89,    85,    89,    84,   518,    89,    68,    69,    85,
    59,    60,    61,   102,   406,     3,   444,     8,     5,     6,
     7,     8,   286,    84,     8,    68,    13,    14,    15,    68,
    69,   408,    68,    76,   767,    85,   521,    47,    81,    89,
   525,    24,   527,    26,    31,    84,    68,    51,    52,    59,
   568,   521,   444,   609,   610,   525,   612,   527,    68,   970,
   521,   972,    85,    86,   525,   966,   527,  1045,    86,   539,
    86,    59,  1072,    47,     3,     4,   444,   967,   968,   549,
    68,    69,    68,    69,   762,    84,   976,   977,   573,  1072,
    68,    69,   577,   521,   579,  1045,   581,   525,    85,   527,
    85,    68,    69,   573,    89,    28,   576,   444,    31,   486,
   968,   539,   573,   444,   792,    77,    78,    79,    47,   977,
    84,   549,    45,    84,    86,   518,    85,   768,   769,   521,
    59,     3,     4,   525,   879,   527,   220,   221,   208,    68,
  1128,  1129,    85,    85,   889,   573,    89,    89,   576,  1045,
  1033,    85,    81,   521,  1066,    89,  1068,   525,   417,   418,
   801,    85,    85,   540,   541,    89,    89,    85,    47,    85,
   280,    89,   657,    89,   659,    47,   661,    10,   663,   268,
   444,   573,   271,   701,   521,    85,   275,    59,   525,    89,
   521,    77,    78,    79,   525,    84,    68,    33,   630,    84,
    86,    77,    78,    79,  1045,   573,     7,     8,   539,    81,
    86,    68,    13,    14,    15,     3,     4,    84,   549,   704,
   738,   706,    72,   708,    90,   710,    84,   712,    10,   714,
    31,   716,     6,   718,    85,   753,   573,    45,  1017,   724,
   967,   968,   573,    85,    76,   576,   316,    77,    78,    79,
    45,    89,    11,    84,   724,   753,    86,   521,    85,   329,
    89,   525,    47,   724,     5,     6,     7,     8,   753,   947,
   340,    91,    13,    14,    15,   539,   346,    47,   348,   764,
    68,    84,   767,   753,   862,   549,    84,   397,    76,    47,
    31,    84,   753,    81,   764,    84,   724,   767,   783,    91,
    84,    84,   820,   764,   774,   946,   767,    84,   826,   573,
     7,     8,   576,   783,    90,    72,    13,    14,    15,    72,
    84,   839,   783,    91,    45,   753,    86,    90,    84,   847,
   762,    86,   724,    84,   423,   424,   764,   850,    86,   767,
    47,   430,     1,    86,    85,    84,   774,   779,   780,    28,
    85,    84,    31,    84,    91,   783,   724,   446,    85,    91,
   792,   753,     7,     8,   862,    85,    45,    28,    13,    14,
    15,   803,   764,    89,    89,   767,   753,   862,    86,   811,
    85,    85,    84,   453,    84,   753,    31,   724,    89,    68,
    69,   783,   862,   724,   879,    84,   764,    72,  1039,   767,
    84,   862,     3,   888,   889,    90,    85,    47,    86,   879,
    89,  1052,    47,    86,    47,   783,   753,    89,   879,   889,
    47,    85,   753,   533,     8,    85,    72,   764,   889,    85,
   767,    85,    47,   764,   862,    85,   767,   441,   442,   528,
    89,   551,   552,   774,    85,    68,   783,    68,    17,    86,
   827,   879,   783,    72,    86,     4,   849,   850,     7,     8,
   724,   889,   839,    85,    13,    14,    15,    47,    68,   846,
   862,    68,    68,    18,    35,    36,    37,    38,    84,    84,
    29,   975,    43,    44,    45,    46,    72,   879,    16,   753,
    18,    19,    20,    21,   862,    85,    84,   889,    85,    84,
   764,    68,    85,   767,    10,    54,    84,   577,    89,   579,
   774,   581,    91,    85,    85,   947,    68,   521,    10,   783,
     5,     6,     7,     8,    84,   862,    85,    76,    13,    14,
    15,   862,    81,    84,    10,   967,   968,    85,   648,   649,
     1,     0,     0,     4,   976,   977,     7,     8,   980,   981,
   516,   413,    13,    14,    15,   549,     5,     6,     7,     8,
   549,   553,   651,   940,    13,    14,    15,   999,    29,   515,
    31,    32,   798,   401,  1072,  1045,     5,     6,     7,     8,
   927,   835,    31,   444,    13,    14,    15,  1072,     7,     8,
   570,   967,  1110,    54,    13,    14,    15,   906,   862,   909,
    85,  1076,  1072,   452,   511,   568,   695,   696,   818,  1078,
   699,  1072,  1127,  1125,   651,    76,   576,  1045,   576,   666,
    81,    35,    36,    37,    38,   769,   641,   631,   632,    43,
    44,    45,    46,   634,  1039,   763,  1013,   999,   703,   572,
   644,   645,   724,    -1,  1072,   572,   586,   471,  1025,    39,
    40,    41,    42,   476,  1031,    -1,    85,    47,    48,    49,
    50,   665,   666,     5,     6,     7,     8,    -1,    -1,   779,
   780,    13,    14,    15,    -1,   679,   680,    -1,    63,    64,
  1072,    66,    67,    68,    69,   246,   247,   248,   249,    31,
    -1,    -1,  1069,  1070,    -1,  1072,    -1,     5,     6,     7,
     8,   811,    -1,    -1,  1072,    13,    14,    15,    57,    58,
    59,    60,    61,    -1,  1045,    -1,   720,    -1,   807,    35,
    36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
    46,    47,    48,    49,    50,  1072,    -1,    -1,    -1,    -1,
    -1,  1072,  1119,    85,    -1,    -1,  1123,    -1,     3,     4,
     5,     6,     7,     8,    -1,    -1,     1,    -1,    -1,     4,
   764,    -1,     7,     8,    -1,    -1,    -1,    -1,    13,    14,
    15,  1148,  1149,   883,   884,    -1,    -1,    85,    -1,    -1,
    -1,  1045,    -1,    -1,    29,    -1,    31,    32,    -1,    -1,
    -1,   880,    -1,   882,    -1,    -1,   885,     5,     6,     7,
     8,   890,   891,    -1,   893,    13,    14,    15,  1072,    54,
    54,    55,    56,    57,    58,    59,    60,    61,   510,   908,
    -1,    76,   911,    31,   913,   914,    81,    -1,    -1,    -1,
    -1,    76,   246,   247,   248,   249,    81,    -1,    -1,    -1,
    85,    86,    53,    54,    55,    56,    57,    58,    59,    60,
    61,   855,   856,    -1,   546,    -1,    -1,   967,   968,    -1,
   250,   251,   252,   253,    -1,    -1,   976,   977,   560,   561,
   980,   981,   433,   434,   435,   436,    -1,    85,    -1,    -1,
    -1,   970,    -1,   972,    -1,     5,     6,     7,     8,   999,
    -1,   895,   896,    13,    14,    15,    -1,    -1,    -1,    -1,
   989,    -1,    -1,    -1,   993,     5,     6,     7,     8,    -1,
   327,   328,    -1,    13,    14,    15,   232,   609,   610,   611,
   612,    55,    56,    57,    58,    59,    60,    61,   345,    -1,
  1040,  1041,  1042,   350,   351,    -1,    -1,    -1,   355,   356,
   357,   358,   359,   360,   361,   362,   363,   364,   365,   366,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,  1046,  1047,    -1,
  1049,    39,    40,    41,    42,  1054,    -1,  1056,    -1,    47,
    48,    49,    50,    -1,     4,  1064,    -1,     7,     8,     5,
     6,     7,     8,    13,    14,    15,    -1,    13,    14,    15,
    -1,    -1,    -1,   685,    -1,   687,    -1,    -1,    -1,    29,
    -1,     1,    -1,    -1,     4,     5,     6,     7,     8,    -1,
    -1,    -1,    -1,    13,    14,    15,    -1,    -1,    -1,   433,
   434,   435,   436,    -1,    54,    -1,    -1,    -1,    -1,    29,
    -1,     1,    -1,     3,     4,    -1,    -1,     7,     8,     9,
    10,    -1,    12,    13,    14,    15,    76,   437,   438,   439,
   440,    81,    -1,    -1,    54,    -1,    -1,    -1,    -1,    29,
    30,    31,    32,    -1,    34,    35,    36,    37,    38,    -1,
    -1,   488,   489,    43,    -1,    -1,    76,    -1,    -1,    49,
    -1,    81,    52,    -1,    54,    -1,    -1,    57,    58,    59,
    90,    -1,    -1,    63,    64,    -1,   657,   658,    68,    69,
   661,   662,    -1,    73,    -1,    75,    76,    -1,    -1,    -1,
    80,    81,    -1,    -1,    -1,    -1,    -1,    87,    88,    -1,
    90,    -1,    92,    -1,    -1,    -1,    -1,     1,    -1,    -1,
     4,     5,     6,     7,     8,    -1,    -1,    -1,    -1,    13,
    14,    15,    -1,   704,   705,   706,   707,    -1,    -1,    -1,
   567,   712,   713,   714,   715,    29,    -1,    31,    -1,    -1,
   852,    -1,    -1,   580,    -1,    -1,    -1,    -1,     5,     6,
     7,     8,   250,   251,   252,   253,    13,    14,    15,    -1,
    54,    -1,   599,     1,    -1,    -1,     4,     5,     6,     7,
     8,    -1,    -1,    -1,    31,    13,    14,    15,    -1,   515,
    -1,    -1,    76,    -1,    -1,    -1,    -1,    81,    -1,    -1,
    28,    29,    -1,    31,   906,    -1,    90,   909,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,     1,    -1,
    -1,     4,     5,     6,     7,     8,    54,    -1,    -1,    -1,
    13,    14,    15,   657,   658,    -1,    -1,   661,   662,    -1,
    68,    69,     5,     6,     7,     8,    29,    -1,    76,    -1,
    13,    14,    15,    81,   681,    -1,    -1,    85,    -1,   659,
   660,    89,    90,   663,   664,    -1,    -1,    -1,    31,    -1,
    -1,    54,    -1,    -1,    -1,    -1,     5,     6,     7,     8,
   704,   705,   706,   707,    13,    14,    15,    -1,   712,   713,
   714,   715,    -1,    76,    -1,    -1,    -1,    -1,    81,    -1,
    -1,    -1,    31,    -1,    -1,    -1,   733,    90,   708,   709,
   710,   711,    -1,    -1,    -1,    -1,   716,   717,   718,   719,
     5,     6,     7,     8,    -1,    -1,  1028,    -1,    13,    14,
    15,   657,   658,   659,   660,   661,   662,   663,   664,    -1,
    -1,    -1,    -1,    -1,     4,     5,     6,     7,     8,   437,
   438,   439,   440,    13,    14,    15,    49,    50,    51,    52,
    53,    54,    55,    56,    57,    58,    59,    60,    61,    29,
    -1,   798,    -1,    -1,    -1,    -1,  1078,    -1,   704,   705,
   706,   707,   708,   709,   710,   711,   712,   713,   714,   715,
   716,   717,   718,   719,    54,   822,    -1,     3,    -1,    -1,
   827,    -1,    -1,     9,    10,    -1,    12,    -1,    -1,   836,
   837,    -1,    -1,  1115,  1116,   842,    76,    -1,    -1,    -1,
    -1,    81,    -1,    -1,    30,    85,    32,    -1,    34,    35,
    36,    37,    38,    -1,    -1,    -1,    -1,    43,    -1,    -1,
    -1,    -1,    -1,    49,   871,    -1,    52,    -1,    -1,    -1,
    -1,    57,    58,    59,    -1,    -1,    -1,    63,    64,    -1,
    -1,    -1,    68,    69,    -1,    -1,    -1,    73,    -1,    75,
    76,    -1,    -1,    -1,    80,   902,    -1,    -1,    -1,    -1,
   907,    87,    88,   910,    -1,    -1,    92,    52,    53,    54,
    55,    56,    57,    58,    59,    60,    61,   924,    -1,    -1,
   927,    -1,   929,   930,    -1,    -1,    -1,    -1,    -1,    -1,
     4,     5,     6,     7,     8,    -1,    -1,    -1,    -1,    13,
    14,    15,     1,    -1,     3,     4,    -1,    -1,    -1,    -1,
     9,    10,    -1,    12,    -1,    29,    -1,    16,   965,    18,
    19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
    -1,    30,    -1,    32,    -1,    34,    35,    36,    37,    38,
    54,    -1,    -1,   990,    43,    -1,    -1,   994,    -1,    -1,
    49,   659,   660,    52,  1001,   663,   664,  1004,    57,    58,
    59,  1008,    76,    -1,    63,    64,    -1,    81,    -1,    68,
    69,    85,    -1,    -1,    73,    -1,    75,    76,    -1,    -1,
    -1,    80,    81,    -1,    -1,    -1,    85,    -1,    87,    88,
    -1,    90,    -1,    92,    -1,    -1,    -1,    -1,    -1,    -1,
   708,   709,   710,   711,    -1,    -1,    -1,    -1,   716,   717,
   718,   719,     1,    -1,     3,     4,     5,     6,     7,     8,
     9,    10,    -1,    12,    13,    14,    15,    16,    -1,    18,
    19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
    29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
    -1,    -1,    -1,    -1,    43,    -1,    -1,    -1,    -1,    -1,
    49,    -1,    -1,    52,    -1,    54,    -1,    -1,    57,    58,
    59,    -1,    -1,    -1,    63,    64,    -1,    -1,    -1,    68,
    69,    -1,    -1,    -1,    73,    -1,    75,    76,    -1,    -1,
    -1,    80,    81,    -1,    -1,    -1,    85,    86,    87,    88,
    -1,    90,     1,    92,     3,     4,     5,     6,     7,     8,
     9,    10,    -1,    12,    13,    14,    15,    16,    -1,    18,
    19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
    29,    30,    31,    32,    -1,    34,    35,    36,    37,    38,
    -1,    -1,    -1,    -1,    43,    -1,    -1,    -1,    -1,    -1,
    49,    -1,    -1,    52,    -1,    54,    -1,    -1,    57,    58,
    59,    -1,    -1,    -1,    63,    64,    -1,    -1,    -1,    68,
    69,    -1,    -1,    -1,    73,    -1,    75,    76,    -1,    -1,
    -1,    80,    81,    -1,    -1,    -1,    85,    -1,    87,    88,
    -1,    90,     1,    92,     3,     4,    -1,    -1,    -1,    -1,
     9,    10,    -1,    12,    -1,    -1,    -1,    16,    -1,    18,
    19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
    -1,    30,    -1,    32,    -1,    34,    35,    36,    37,    38,
    -1,    -1,    -1,    -1,    43,    -1,    -1,    -1,    -1,    -1,
    49,    -1,    -1,    52,    -1,    -1,    -1,    -1,    57,    58,
    59,    -1,    -1,    -1,    63,    64,    -1,    -1,    -1,    68,
    69,    -1,    -1,    -1,    73,    -1,    75,    76,    -1,    -1,
    -1,    80,    81,    -1,    -1,    -1,    85,    -1,    87,    88,
    -1,    90,     1,    92,     3,     4,    -1,    -1,    -1,    -1,
     9,    10,    46,    12,    48,    49,    50,    51,    52,    53,
    54,    55,    56,    57,    58,    59,    60,    61,    -1,    -1,
    -1,    30,    -1,    32,    -1,    34,    35,    36,    37,    38,
    -1,    -1,    -1,    -1,    43,    -1,    -1,    -1,    -1,    -1,
    49,    -1,    -1,    52,    -1,    -1,    -1,    -1,    57,    58,
    59,    -1,    -1,    -1,    63,    64,    -1,    -1,    67,    68,
    69,    -1,    -1,    -1,    73,    -1,    75,    76,    -1,    -1,
    -1,    80,    81,    -1,    -1,    -1,    -1,    86,    87,    88,
    -1,    90,     1,    92,     3,     4,    -1,    -1,    -1,    -1,
     9,    10,    -1,    12,    50,    51,    52,    53,    54,    55,
    56,    57,    58,    59,    60,    61,    -1,    -1,    -1,    -1,
    -1,    30,    -1,    32,    -1,    34,    35,    36,    37,    38,
    -1,    -1,    -1,    -1,    43,    -1,    -1,    -1,    -1,    -1,
    49,    -1,    -1,    52,    -1,    -1,    -1,    -1,    57,    58,
    59,    -1,    -1,    -1,    63,    64,    -1,    -1,    67,    68,
    69,    -1,    -1,    -1,    73,    -1,    75,    76,    -1,    -1,
    -1,    80,    81,    -1,    -1,    -1,    -1,    86,    87,    88,
     1,    90,     3,    92,    -1,    -1,    -1,    -1,     9,    10,
    -1,    12,    51,    52,    53,    54,    55,    56,    57,    58,
    59,    60,    61,    -1,    -1,    -1,    -1,    -1,    -1,    30,
    -1,    32,    -1,    34,    35,    36,    37,    38,    -1,    -1,
    -1,    -1,    43,    -1,    45,    -1,    -1,    -1,    49,    -1,
    -1,    52,    -1,    -1,    -1,    -1,    57,    58,    59,    -1,
    -1,    -1,    63,    64,    -1,    -1,    67,    68,    69,     1,
    -1,     3,    73,    -1,    75,    -1,    -1,     9,    10,    80,
    12,    -1,    -1,    -1,    -1,    -1,    87,    88,    -1,    90,
    -1,    92,    -1,    -1,    -1,    -1,    -1,    -1,    30,    -1,
    32,    -1,    34,    35,    36,    37,    38,    -1,    -1,    -1,
    -1,    43,    -1,    -1,    -1,    -1,    -1,    49,    -1,    -1,
    52,    -1,    -1,    -1,    -1,    57,    58,    59,    -1,    -1,
    -1,    63,    64,    -1,    -1,    -1,    68,    69,     1,    -1,
     3,    73,    -1,    75,    -1,    -1,     9,    10,    80,    12,
    -1,    -1,    -1,    -1,    -1,    87,    88,    -1,    90,    -1,
    92,    -1,    -1,    -1,    -1,    -1,    -1,    30,    -1,    32,
    -1,    34,    35,    36,    37,    38,    -1,    -1,    -1,    -1,
    43,    -1,    -1,    -1,    -1,    -1,    49,    -1,    -1,    52,
    -1,    -1,    -1,    -1,    57,    58,    59,    -1,    -1,    -1,
    63,    64,    -1,    -1,    -1,    68,    69,    -1,    -1,    -1,
    73,    -1,    75,    -1,    -1,    -1,    -1,    80,    -1,    -1,
    -1,    -1,    -1,    -1,    87,    88,    -1,    90,    -1,    92,
     3,     4,     5,     6,     7,     8,     9,    10,    -1,    12,
    13,    14,    15,    -1,    -1,    -1,    -1,    -1,    -1,    22,
    23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
    -1,    34,    35,    36,    37,    38,    -1,    -1,    -1,    -1,
    43,    -1,    -1,    -1,    -1,    -1,    49,    -1,    -1,    52,
    -1,    54,    -1,    -1,    57,    58,    59,    -1,    -1,    -1,
    63,    64,    -1,    -1,    -1,    68,    69,    -1,    -1,    -1,
    73,    -1,    75,    76,    -1,    -1,    -1,    80,    81,    -1,
    -1,    -1,    85,    -1,    87,    88,    -1,    90,    -1,    92,
     3,     4,     5,     6,     7,     8,     9,    10,    -1,    12,
    13,    14,    15,    -1,     4,     5,     6,     7,     8,    -1,
    -1,    11,    -1,    13,    14,    15,    29,    30,    31,    32,
    -1,    34,    35,    36,    37,    38,    -1,    -1,    -1,    29,
    43,    31,    -1,    -1,    -1,    -1,    49,    -1,    -1,    52,
    -1,    54,    -1,    -1,    57,    58,    59,    -1,    -1,    -1,
    63,    64,    -1,    -1,    54,    68,    69,    -1,    -1,    -1,
    73,    -1,    75,    76,     3,     4,    -1,    80,    81,    -1,
     9,    10,    -1,    12,    87,    88,    76,    -1,    -1,    92,
    -1,    81,    -1,    22,    23,    24,    25,    26,    27,    28,
    -1,    30,    -1,    32,    -1,    34,    35,    36,    37,    38,
    -1,    -1,    -1,    -1,    43,    -1,    -1,    -1,    -1,    -1,
    49,    -1,    -1,    52,    -1,    -1,    -1,    -1,    57,    58,
    59,    -1,    -1,    -1,    63,    64,    -1,    -1,    -1,    68,
    69,    -1,    -1,    -1,    73,    -1,    75,    76,    -1,    -1,
    -1,    80,    81,    -1,    -1,    -1,    85,    -1,    87,    88,
    -1,    90,    -1,    92,     3,     4,    -1,    -1,     7,     8,
     9,    10,    -1,    12,    13,    14,    15,    -1,     4,     5,
     6,     7,     8,    -1,    -1,    -1,    -1,    13,    14,    15,
    29,    30,    31,    32,    -1,    34,    35,    36,    37,    38,
    -1,    -1,    -1,    29,    43,    31,    -1,    -1,    -1,    -1,
    49,    -1,    -1,    52,    -1,    54,    -1,    -1,    57,    58,
    59,    -1,    -1,    -1,    63,    64,    -1,    -1,    54,    68,
    69,    -1,    -1,    -1,    73,    -1,    75,    76,     3,    -1,
    -1,    80,    81,    -1,     9,    10,    -1,    12,    87,    88,
    76,    -1,    -1,    92,    -1,    81,    -1,    -1,    -1,    85,
    -1,    -1,    -1,    -1,    -1,    30,    -1,    32,    -1,    34,
    35,    36,    37,    38,    -1,    -1,    -1,    -1,    43,    -1,
    -1,    -1,    -1,    -1,    49,    -1,    -1,    52,    -1,    -1,
    -1,    -1,    57,    58,    59,    -1,    -1,    -1,    63,    64,
    -1,    -1,    -1,    68,    69,     3,    -1,    -1,    73,    -1,
    75,     9,    10,    -1,    12,    80,    -1,    -1,    -1,    -1,
    -1,    -1,    87,    88,    -1,    -1,    91,    92,    -1,    -1,
    -1,    -1,    30,    -1,    32,    -1,    34,    35,    36,    37,
    38,    -1,    -1,    -1,    -1,    43,    -1,    -1,    -1,    -1,
    -1,    49,    -1,    -1,    52,    -1,    -1,    -1,    -1,    57,
    58,    59,    -1,    -1,    -1,    63,    64,    -1,    -1,    -1,
    68,    69,     3,    -1,    -1,    73,    -1,    75,     9,    10,
    -1,    12,    80,    -1,    -1,    -1,    -1,    -1,    -1,    87,
    88,    -1,    90,    -1,    92,    -1,    -1,    -1,    -1,    30,
    -1,    32,    -1,    34,    35,    36,    37,    38,    -1,    -1,
    -1,    -1,    43,    -1,    -1,    -1,    -1,    -1,    49,    -1,
    -1,    52,    -1,    -1,    -1,    -1,    57,    58,    59,    -1,
    -1,    -1,    63,    64,    -1,    -1,    -1,    68,    69,     3,
    -1,    -1,    73,    -1,    75,     9,    10,    -1,    12,    80,
    -1,    -1,    -1,    -1,    -1,    -1,    87,    88,    -1,    -1,
    91,    92,    -1,    -1,    -1,    -1,    30,    -1,    32,    -1,
    34,    35,    36,    37,    38,    -1,    -1,    -1,    -1,    43,
    -1,    -1,    -1,    -1,    -1,    49,    -1,    -1,    52,    -1,
    -1,    -1,    -1,    57,    58,    59,    -1,    -1,    -1,    63,
    64,    -1,    -1,    -1,    68,    69,     3,    -1,    -1,    73,
    -1,    75,     9,    10,    -1,    12,    80,    -1,    -1,    -1,
    -1,    85,    -1,    87,    88,    -1,    -1,    -1,    92,    -1,
    -1,    -1,    -1,    30,    -1,    32,    -1,    34,    35,    36,
    37,    38,    -1,    -1,    -1,    -1,    43,    -1,    -1,    -1,
    -1,    -1,    49,    -1,    -1,    52,    -1,    -1,    -1,    -1,
    57,    58,    59,    -1,    -1,    -1,    63,    64,    -1,    -1,
    -1,    68,    69,     3,    -1,    -1,    73,    -1,    75,     9,
    10,    -1,    12,    80,    -1,    -1,    -1,    -1,    -1,    -1,
    87,    88,    -1,    -1,    -1,    92,    -1,    -1,    -1,    -1,
    30,    -1,    32,    -1,    34,    35,    36,    37,    38,    -1,
    -1,    -1,    -1,    43,    -1,    -1,    -1,    -1,    -1,    49,
    -1,    -1,    52,    -1,    -1,    -1,    -1,    57,    58,    59,
    -1,    -1,    -1,    63,    64,    -1,    -1,    -1,    68,    69,
     3,    -1,    -1,    73,    -1,    75,     9,    10,    -1,    12,
    80,    -1,    -1,    -1,    -1,    -1,    -1,    87,    88,    -1,
    -1,    -1,    92,    -1,    -1,    -1,    -1,    30,    -1,    32,
    -1,    34,    35,    36,    37,    38,    -1,    -1,    -1,    -1,
    43,    -1,    -1,    -1,    -1,    -1,    49,    -1,    -1,    52,
    -1,    -1,    -1,    -1,    57,    58,    59,    -1,    -1,    -1,
    63,    64,    -1,    -1,    -1,    68,    69,     3,    -1,    -1,
    73,    -1,    75,     9,    10,    -1,    12,    80,    -1,    -1,
    -1,    -1,    -1,    -1,    87,    88,    -1,    -1,    -1,    92,
    -1,    -1,    -1,    -1,    30,    -1,    32,    -1,    34,    35,
    36,    37,    38,    -1,    -1,    -1,    -1,    43,    -1,    -1,
    -1,    -1,    -1,    49,    -1,    -1,    52,    -1,    -1,    -1,
    -1,    57,    58,    59,    -1,    -1,    -1,    63,    64,    -1,
    -1,    -1,    68,    69,    -1,     1,    -1,    73,     4,    75,
    -1,     7,     8,    -1,    80,    -1,    -1,    13,    14,    15,
    -1,    87,    88,    -1,     1,    -1,    92,     4,     5,     6,
     7,     8,    -1,    29,    -1,    31,    13,    14,    15,    -1,
    -1,    -1,    -1,     1,    -1,    -1,     4,     5,     6,     7,
     8,    28,    29,    -1,    31,    13,    14,    15,    54,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,
    28,    29,    -1,    31,    -1,    -1,    -1,    54,    -1,    -1,
    76,    77,    78,    79,    -1,    81,    -1,    45,    -1,    85,
    86,    68,    69,    -1,    -1,    -1,    54,    -1,    -1,    76,
    -1,    -1,    -1,    -1,    81,    -1,    -1,    -1,    85,    -1,
    -1,    -1,    89,    90,    -1,    -1,    -1,    -1,    76,    -1,
    -1,    -1,    -1,    81,    -1,    -1,    -1,    85,    -1,    -1,
     1,    89,    90,     4,     5,     6,     7,     8,    -1,    -1,
    -1,    -1,    13,    14,    15,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,     3,     4,    -1,    -1,     7,     8,    29,    -1,
    31,    12,    13,    14,    15,    16,    17,    18,    19,    20,
    21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
    -1,     1,    -1,    54,     4,    -1,    -1,     7,     8,    -1,
    -1,    -1,    -1,    13,    14,    15,    47,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    76,    -1,    -1,    -1,    29,
    81,    31,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    90,
    -1,    -1,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,
    81,    -1,    -1,    -1,    54,    -1,    -1,    -1,    89,    -1,
     1,    -1,     3,     4,     5,     6,     7,     8,    -1,    -1,
    -1,    -1,    13,    14,    15,    -1,    76,    77,    78,    79,
    -1,    81,    -1,    -1,    -1,    85,    86,    28,    29,    -1,
    31,    32,     1,    -1,     3,     4,     5,     6,     7,     8,
    -1,    -1,    -1,    -1,    13,    14,    15,    -1,    -1,    -1,
    -1,    -1,    -1,    54,    -1,    -1,    57,    58,    59,    -1,
    29,    -1,    31,    -1,    -1,    -1,    -1,    68,    -1,    70,
    71,    72,    -1,    -1,    -1,    76,    -1,    -1,    -1,    80,
    81,    82,    83,    -1,    85,    54,    -1,    -1,    57,    58,
    59,     1,    -1,     3,     4,     5,     6,     7,     8,    68,
    -1,    -1,    -1,    13,    14,    15,    -1,    76,    -1,    -1,
    -1,    -1,    81,    -1,    -1,    -1,    85,    -1,    -1,    29,
     1,    31,     3,     4,     5,     6,     7,     8,    -1,    -1,
    11,    -1,    13,    14,    15,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    54,    -1,    -1,    -1,    29,    59,
    -1,     4,     5,     6,     7,     8,    -1,    -1,    68,    -1,
    13,    14,    15,    -1,    -1,    -1,    76,    -1,    -1,    -1,
    -1,    81,    -1,    54,     1,    85,    29,     4,     5,     6,
     7,     8,    -1,    -1,    11,    -1,    13,    14,    15,    -1,
    -1,    -1,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,
    81,    54,    29,    84,    -1,     4,     5,     6,     7,     8,
    -1,    -1,    -1,    -1,    13,    14,    15,    -1,    -1,    -1,
    -1,    -1,    -1,    76,    -1,    -1,    -1,    54,    81,    -1,
    29,    -1,    31,     4,     5,     6,     7,     8,    -1,    -1,
    -1,    -1,    13,    14,    15,    -1,    -1,    -1,    -1,    76,
    -1,    -1,    -1,    -1,    81,    54,    -1,    84,    29,    -1,
    31,    -1,     4,     5,     6,     7,     8,    -1,    -1,    -1,
    -1,    13,    14,    15,    -1,    -1,    -1,    76,    -1,    -1,
    -1,    -1,    81,    54,    -1,    -1,    85,    29,    -1,    31,
     4,     5,     6,     7,     8,    -1,    -1,    -1,    -1,    13,
    14,    15,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,
    81,    11,    54,    -1,    85,    29,     4,     5,     6,     7,
     8,    -1,    -1,    -1,    -1,    13,    14,    15,    -1,    -1,
    -1,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    81,
    54,    29,    -1,    85,    44,    45,    46,    -1,    48,    49,
    50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
    60,    61,    76,    -1,    -1,    -1,    54,    81,    44,    45,
    46,    85,    48,    49,    50,    51,    52,    53,    54,    55,
    56,    57,    58,    59,    60,    61,    -1,    -1,    76,    -1,
    -1,    91,    -1,    81,     3,     4,    -1,    85,     7,     8,
    -1,    -1,    -1,    12,    13,    14,    15,    16,    17,    18,
    19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
    29,    30,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     3,     4,    -1,    -1,     7,     8,    -1,    -1,    47,    12,
    13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
    23,    24,    25,    26,    27,    28,    29,    30,    -1,    68,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    -1,    -1,
    -1,    -1,    81,    -1,    47,     3,     4,    -1,    -1,     7,
     8,    -1,    -1,    -1,    12,    13,    14,    15,    16,    17,
    18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
    28,    29,    30,    76,    -1,    -1,    -1,    -1,    81,    -1,
    -1,     3,     4,    -1,    -1,     7,     8,    -1,    -1,    47,
    12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
    22,    23,    24,    25,    26,    27,    28,    29,    30,    -1,
     4,     5,     6,     7,     8,    -1,    -1,    11,    76,    13,
    14,    15,    -1,    81,    -1,    47,    -1,    -1,     4,     5,
     6,     7,     8,    -1,    -1,    29,    -1,    13,    14,    15,
     4,     5,     6,     7,     8,    -1,    -1,    -1,    -1,    13,
    14,    15,    -1,    29,    76,    31,    -1,    -1,    -1,    81,
    54,    -1,    -1,    -1,    -1,    29,    -1,    31,    -1,    -1,
     4,     5,     6,     7,     8,    -1,    -1,    -1,    54,    13,
    14,    15,    76,    -1,    -1,    -1,    -1,    81,    -1,    -1,
    54,    -1,    -1,    -1,    -1,    29,    -1,    31,    -1,    -1,
    76,    -1,    -1,    -1,    -1,    81,    -1,    -1,    -1,    -1,
    -1,    -1,    76,    -1,    -1,    -1,    -1,    81,    -1,    -1,
    54,     4,     5,     6,     7,     8,    -1,    -1,    -1,    -1,
    13,    14,    15,    -1,     4,     5,     6,     7,     8,    -1,
    -1,    -1,    76,    13,    14,    15,    29,    81,    31,    -1,
    -1,    -1,    -1,     4,     5,     6,     7,     8,    -1,    29,
    -1,    31,    13,    14,    15,     4,     5,     6,     7,     8,
    -1,    54,    -1,    -1,    13,    14,    15,    -1,    29,    -1,
    -1,    -1,    -1,    -1,    54,     4,     5,     6,     7,     8,
    29,    -1,    -1,    76,    13,    14,    15,    -1,    81,    -1,
    -1,    -1,    -1,    54,    -1,    -1,    76,    -1,    -1,    -1,
    29,    81,    -1,    -1,    -1,    54,    -1,    -1,     4,     5,
     6,     7,     8,    -1,    -1,    76,    -1,    13,    14,    15,
    81,    -1,    -1,    -1,    -1,    54,     4,    76,    -1,     7,
     8,    -1,    81,    29,    -1,    13,    14,    15,    -1,     4,
    -1,    -1,     7,     8,    -1,    -1,    -1,    76,    13,    14,
    15,    29,    81,    31,    -1,    -1,    -1,    -1,    54,     4,
    -1,    -1,     7,     8,    29,    -1,    31,    -1,    13,    14,
    15,    -1,    -1,    -1,    -1,    -1,    54,    -1,    -1,    -1,
    76,    -1,    -1,    -1,    29,    81,    31,    -1,    -1,    54,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    -1,
    -1,    -1,    -1,    81,    11,    -1,    -1,    -1,    -1,    54,
    -1,    76,    -1,    44,    45,    46,    81,    48,    49,    50,
    51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
    61,    76,    -1,    -1,    -1,    -1,    81,    44,    45,    46,
    47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
    57,    58,    59,    60,    61,    -1,    -1,    44,    45,    46,
    91,    48,    49,    50,    51,    52,    53,    54,    55,    56,
    57,    58,    59,    60,    61,    44,    45,    46,    -1,    48,
    49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
    59,    60,    61,    -1,    -1,    -1,    -1,    -1,    -1,    44,
    45,    46,    89,    48,    49,    50,    51,    52,    53,    54,
    55,    56,    57,    58,    59,    60,    61,    44,    45,    46,
    89,    48,    49,    50,    51,    52,    53,    54,    55,    56,
    57,    58,    59,    60,    61,    -1,    -1,    -1,    -1,    -1,
    -1,    31,    -1,    -1,    89,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    44,    45,    46,    84,    48,    49,
    50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
    60,    61,    44,    45,    46,    47,    48,    49,    50,    51,
    52,    53,    54,    55,    56,    57,    58,    59,    60,    61
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/local/share/bison.simple"

/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

#ifndef alloca
#ifdef __GNUC__
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi)
#include <alloca.h>
#else /* not sparc */
#if defined (MSDOS) && !defined (__TURBOC__)
#include <malloc.h>
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
#include <malloc.h>
 #pragma alloca
#else /* not MSDOS, __TURBOC__, or _AIX */
#ifdef __hpux
#ifdef __cplusplus
extern "C" {
void *alloca (unsigned int);
};
#else /* not __cplusplus */
void *alloca ();
#endif /* not __cplusplus */
#endif /* __hpux */
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc.  */
#endif /* not GNU C.  */
#endif /* alloca not defined.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	return(0)
#define YYABORT 	return(1)
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    { yychar = (token), yylval = (value);			\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { yyerror ("syntax error: cannot back up"); YYERROR; }	\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

#ifndef YYPURE
#define YYLEX		yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int	yychar;			/*  the lookahead symbol		*/
YYSTYPE	yylval;			/*  the semantic value of the		*/
				/*  lookahead symbol			*/

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;			/*  location data for the lookahead	*/
				/*  symbol				*/
#endif

int yynerrs;			/*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;			/*  nonzero means print parse trace	*/
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks	*/

#ifndef	YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
int yyparse (void);
#endif

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(TO,FROM,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (to, from, count)
     char *to;
     char *from;
     int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *to, char *from, int count)
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 196 "/usr/local/share/bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#ifdef __cplusplus
#define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else /* not __cplusplus */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#endif /* not __cplusplus */
#else /* not YYPARSE_PARAM */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif /* not YYPARSE_PARAM */

int
yyparse(YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;	/*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;		/*  lookahead token as an internal (translated) token number */

  short	yyssa[YYINITDEPTH];	/*  the state stack			*/
  YYSTYPE yyvsa[YYINITDEPTH];	/*  the semantic value stack		*/

  short *yyss = yyssa;		/*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;	/*  to allow yyoverflow to reallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];	/*  the location stack			*/
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;		/*  the variable used to return		*/
				/*  semantic values from the action	*/
				/*  routines				*/

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to reallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
	 the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
	 but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yyls1, size * sizeof (*yylsp),
		 &yystacksize);
#else
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	{
	  yyerror("parser stack overflow");
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
      yyss = (short *) alloca (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss, (char *)yyss1, size * sizeof (*yyssp));
      yyvs = (YYSTYPE *) alloca (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1, size * sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) alloca (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1, size * sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
 yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
	{
	  fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise meaning
	     of a token, for further debugging info.  */
#ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
#endif
	  fprintf (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
	       yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
	fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif


  switch (yyn) {

case 1:
#line 345 "objc-parse.y"
{ if (pedantic)
		    pedwarn ("ISO C forbids an empty source file");
		  finish_file ();
		;
    break;}
case 2:
#line 350 "objc-parse.y"
{
		  /* In case there were missing closebraces,
		     get us back to the global binding level.  */
		  while (! global_bindings_p ())
		    poplevel (0, 0, 0);
		  /* __FUNCTION__ is defined at file scope ("").  This
		     call may not be necessary as my tests indicate it
		     still works without it.  */
		  finish_fname_decls ();
                  finish_file ();
		;
    break;}
case 3:
#line 368 "objc-parse.y"
{yyval.ttype = NULL_TREE; ;
    break;}
case 5:
#line 369 "objc-parse.y"
{yyval.ttype = NULL_TREE; ggc_collect(); ;
    break;}
case 7:
#line 374 "objc-parse.y"
{ parsing_iso_function_signature = false; ;
    break;}
case 11:
#line 382 "objc-parse.y"
{ STRIP_NOPS (yyvsp[-2].ttype);
		  if ((TREE_CODE (yyvsp[-2].ttype) == ADDR_EXPR
		       && TREE_CODE (TREE_OPERAND (yyvsp[-2].ttype, 0)) == STRING_CST)
		      || TREE_CODE (yyvsp[-2].ttype) == STRING_CST)
		    assemble_asm (yyvsp[-2].ttype);
		  else
		    error ("argument of `asm' is not a constant string"); ;
    break;}
case 12:
#line 390 "objc-parse.y"
{ RESTORE_EXT_FLAGS (yyvsp[-1].ttype); ;
    break;}
case 13:
#line 395 "objc-parse.y"
{ if (pedantic)
		    error ("ISO C forbids data definition with no type or storage class");
		  else
		    warning ("data definition has no type or storage class");

		  POP_DECLSPEC_STACK; ;
    break;}
case 14:
#line 402 "objc-parse.y"
{ POP_DECLSPEC_STACK; ;
    break;}
case 15:
#line 404 "objc-parse.y"
{ POP_DECLSPEC_STACK; ;
    break;}
case 16:
#line 406 "objc-parse.y"
{ shadow_tag (yyvsp[-1].ttype); ;
    break;}
case 19:
#line 410 "objc-parse.y"
{ if (pedantic)
		    pedwarn ("ISO C does not allow extra `;' outside of a function"); ;
    break;}
case 20:
#line 416 "objc-parse.y"
{ if (! start_function (current_declspecs, yyvsp[0].ttype,
					all_prefix_attributes))
		    YYERROR1;
		;
    break;}
case 21:
#line 421 "objc-parse.y"
{ store_parm_decls (); ;
    break;}
case 22:
#line 423 "objc-parse.y"
{ DECL_SOURCE_FILE (current_function_decl) = yyvsp[-2].filename;
		  DECL_SOURCE_LINE (current_function_decl) = yyvsp[-1].lineno;
		  finish_function (0, 1);
		  POP_DECLSPEC_STACK; ;
    break;}
case 23:
#line 428 "objc-parse.y"
{ POP_DECLSPEC_STACK; ;
    break;}
case 24:
#line 430 "objc-parse.y"
{ if (! start_function (current_declspecs, yyvsp[0].ttype,
					all_prefix_attributes))
		    YYERROR1;
		;
    break;}
case 25:
#line 435 "objc-parse.y"
{ store_parm_decls (); ;
    break;}
case 26:
#line 437 "objc-parse.y"
{ DECL_SOURCE_FILE (current_function_decl) = yyvsp[-2].filename;
		  DECL_SOURCE_LINE (current_function_decl) = yyvsp[-1].lineno;
		  finish_function (0, 1);
		  POP_DECLSPEC_STACK; ;
    break;}
case 27:
#line 442 "objc-parse.y"
{ POP_DECLSPEC_STACK; ;
    break;}
case 28:
#line 444 "objc-parse.y"
{ if (! start_function (NULL_TREE, yyvsp[0].ttype,
					all_prefix_attributes))
		    YYERROR1;
		;
    break;}
case 29:
#line 449 "objc-parse.y"
{ store_parm_decls (); ;
    break;}
case 30:
#line 451 "objc-parse.y"
{ DECL_SOURCE_FILE (current_function_decl) = yyvsp[-2].filename;
		  DECL_SOURCE_LINE (current_function_decl) = yyvsp[-1].lineno;
		  finish_function (0, 1);
		  POP_DECLSPEC_STACK; ;
    break;}
case 31:
#line 456 "objc-parse.y"
{ POP_DECLSPEC_STACK; ;
    break;}
case 36:
#line 467 "objc-parse.y"
{ yyval.code = ADDR_EXPR; ;
    break;}
case 37:
#line 469 "objc-parse.y"
{ yyval.code = NEGATE_EXPR; ;
    break;}
case 38:
#line 471 "objc-parse.y"
{ yyval.code = CONVERT_EXPR;
		;
    break;}
case 39:
#line 474 "objc-parse.y"
{ yyval.code = PREINCREMENT_EXPR; ;
    break;}
case 40:
#line 476 "objc-parse.y"
{ yyval.code = PREDECREMENT_EXPR; ;
    break;}
case 41:
#line 478 "objc-parse.y"
{ yyval.code = BIT_NOT_EXPR; ;
    break;}
case 42:
#line 480 "objc-parse.y"
{ yyval.code = TRUTH_NOT_EXPR; ;
    break;}
case 43:
#line 484 "objc-parse.y"
{ yyval.ttype = build_compound_expr (yyvsp[0].ttype); ;
    break;}
case 44:
#line 489 "objc-parse.y"
{ yyval.ttype = NULL_TREE; ;
    break;}
case 46:
#line 495 "objc-parse.y"
{ yyval.ttype = build_tree_list (NULL_TREE, yyvsp[0].ttype); ;
    break;}
case 47:
#line 497 "objc-parse.y"
{ chainon (yyvsp[-2].ttype, build_tree_list (NULL_TREE, yyvsp[0].ttype)); ;
    break;}
case 49:
#line 503 "objc-parse.y"
{ yyval.ttype = build_indirect_ref (yyvsp[0].ttype, "unary *"); ;
    break;}
case 50:
#line 506 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype;
		  RESTORE_EXT_FLAGS (yyvsp[-1].ttype); ;
    break;}
case 51:
#line 509 "objc-parse.y"
{ yyval.ttype = build_unary_op (yyvsp[-1].code, yyvsp[0].ttype, 0);
		  overflow_warning (yyval.ttype); ;
    break;}
case 52:
#line 513 "objc-parse.y"
{ yyval.ttype = finish_label_address_expr (yyvsp[0].ttype); ;
    break;}
case 53:
#line 515 "objc-parse.y"
{ skip_evaluation--;
		  if (TREE_CODE (yyvsp[0].ttype) == COMPONENT_REF
		      && DECL_C_BIT_FIELD (TREE_OPERAND (yyvsp[0].ttype, 1)))
		    error ("`sizeof' applied to a bit-field");
		  yyval.ttype = c_sizeof (TREE_TYPE (yyvsp[0].ttype)); ;
    break;}
case 54:
#line 521 "objc-parse.y"
{ skip_evaluation--;
		  yyval.ttype = c_sizeof (groktypename (yyvsp[-1].ttype)); ;
    break;}
case 55:
#line 524 "objc-parse.y"
{ skip_evaluation--;
		  yyval.ttype = c_alignof_expr (yyvsp[0].ttype); ;
    break;}
case 56:
#line 527 "objc-parse.y"
{ skip_evaluation--;
		  yyval.ttype = c_alignof (groktypename (yyvsp[-1].ttype)); ;
    break;}
case 57:
#line 530 "objc-parse.y"
{ yyval.ttype = build_unary_op (REALPART_EXPR, yyvsp[0].ttype, 0); ;
    break;}
case 58:
#line 532 "objc-parse.y"
{ yyval.ttype = build_unary_op (IMAGPART_EXPR, yyvsp[0].ttype, 0); ;
    break;}
case 59:
#line 536 "objc-parse.y"
{ skip_evaluation++; ;
    break;}
case 60:
#line 540 "objc-parse.y"
{ skip_evaluation++; ;
    break;}
case 61:
#line 544 "objc-parse.y"
{ skip_evaluation++; ;
    break;}
case 63:
#line 550 "objc-parse.y"
{ yyval.ttype = c_cast_expr (yyvsp[-2].ttype, yyvsp[0].ttype); ;
    break;}
case 65:
#line 556 "objc-parse.y"
{ yyval.ttype = parser_build_binary_op (yyvsp[-1].code, yyvsp[-2].ttype, yyvsp[0].ttype); ;
    break;}
case 66:
#line 558 "objc-parse.y"
{ yyval.ttype = parser_build_binary_op (yyvsp[-1].code, yyvsp[-2].ttype, yyvsp[0].ttype); ;
    break;}
case 67:
#line 560 "objc-parse.y"
{ yyval.ttype = parser_build_binary_op (yyvsp[-1].code, yyvsp[-2].ttype, yyvsp[0].ttype); ;
    break;}
case 68:
#line 562 "objc-parse.y"
{ yyval.ttype = parser_build_binary_op (yyvsp[-1].code, yyvsp[-2].ttype, yyvsp[0].ttype); ;
    break;}
case 69:
#line 564 "objc-parse.y"
{ yyval.ttype = parser_build_binary_op (yyvsp[-1].code, yyvsp[-2].ttype, yyvsp[0].ttype); ;
    break;}
case 70:
#line 566 "objc-parse.y"
{ yyval.ttype = parser_build_binary_op (yyvsp[-1].code, yyvsp[-2].ttype, yyvsp[0].ttype); ;
    break;}
case 71:
#line 568 "objc-parse.y"
{ yyval.ttype = parser_build_binary_op (yyvsp[-1].code, yyvsp[-2].ttype, yyvsp[0].ttype); ;
    break;}
case 72:
#line 570 "objc-parse.y"
{ yyval.ttype = parser_build_binary_op (yyvsp[-1].code, yyvsp[-2].ttype, yyvsp[0].ttype); ;
    break;}
case 73:
#line 572 "objc-parse.y"
{ yyval.ttype = parser_build_binary_op (yyvsp[-1].code, yyvsp[-2].ttype, yyvsp[0].ttype); ;
    break;}
case 74:
#line 574 "objc-parse.y"
{ yyval.ttype = parser_build_binary_op (yyvsp[-1].code, yyvsp[-2].ttype, yyvsp[0].ttype); ;
    break;}
case 75:
#line 576 "objc-parse.y"
{ yyval.ttype = parser_build_binary_op (yyvsp[-1].code, yyvsp[-2].ttype, yyvsp[0].ttype); ;
    break;}
case 76:
#line 578 "objc-parse.y"
{ yyval.ttype = parser_build_binary_op (yyvsp[-1].code, yyvsp[-2].ttype, yyvsp[0].ttype); ;
    break;}
case 77:
#line 580 "objc-parse.y"
{ yyvsp[-1].ttype = c_common_truthvalue_conversion
		    (default_conversion (yyvsp[-1].ttype));
		  skip_evaluation += yyvsp[-1].ttype == boolean_false_node; ;
    break;}
case 78:
#line 584 "objc-parse.y"
{ skip_evaluation -= yyvsp[-3].ttype == boolean_false_node;
		  yyval.ttype = parser_build_binary_op (TRUTH_ANDIF_EXPR, yyvsp[-3].ttype, yyvsp[0].ttype); ;
    break;}
case 79:
#line 587 "objc-parse.y"
{ yyvsp[-1].ttype = c_common_truthvalue_conversion
		    (default_conversion (yyvsp[-1].ttype));
		  skip_evaluation += yyvsp[-1].ttype == boolean_true_node; ;
    break;}
case 80:
#line 591 "objc-parse.y"
{ skip_evaluation -= yyvsp[-3].ttype == boolean_true_node;
		  yyval.ttype = parser_build_binary_op (TRUTH_ORIF_EXPR, yyvsp[-3].ttype, yyvsp[0].ttype); ;
    break;}
case 81:
#line 594 "objc-parse.y"
{ yyvsp[-1].ttype = c_common_truthvalue_conversion
		    (default_conversion (yyvsp[-1].ttype));
		  skip_evaluation += yyvsp[-1].ttype == boolean_false_node; ;
    break;}
case 82:
#line 598 "objc-parse.y"
{ skip_evaluation += ((yyvsp[-4].ttype == boolean_true_node)
				      - (yyvsp[-4].ttype == boolean_false_node)); ;
    break;}
case 83:
#line 601 "objc-parse.y"
{ skip_evaluation -= yyvsp[-6].ttype == boolean_true_node;
		  yyval.ttype = build_conditional_expr (yyvsp[-6].ttype, yyvsp[-3].ttype, yyvsp[0].ttype); ;
    break;}
case 84:
#line 604 "objc-parse.y"
{ if (pedantic)
		    pedwarn ("ISO C forbids omitting the middle term of a ?: expression");
		  /* Make sure first operand is calculated only once.  */
		  yyvsp[0].ttype = save_expr (yyvsp[-1].ttype);
		  yyvsp[-1].ttype = c_common_truthvalue_conversion
		    (default_conversion (yyvsp[0].ttype));
		  skip_evaluation += yyvsp[-1].ttype == boolean_true_node; ;
    break;}
case 85:
#line 612 "objc-parse.y"
{ skip_evaluation -= yyvsp[-4].ttype == boolean_true_node;
		  yyval.ttype = build_conditional_expr (yyvsp[-4].ttype, yyvsp[-3].ttype, yyvsp[0].ttype); ;
    break;}
case 86:
#line 615 "objc-parse.y"
{ char class;
		  yyval.ttype = build_modify_expr (yyvsp[-2].ttype, NOP_EXPR, yyvsp[0].ttype);
		  class = TREE_CODE_CLASS (TREE_CODE (yyval.ttype));
		  if (IS_EXPR_CODE_CLASS (class))
		    C_SET_EXP_ORIGINAL_CODE (yyval.ttype, MODIFY_EXPR);
		;
    break;}
case 87:
#line 622 "objc-parse.y"
{ char class;
		  yyval.ttype = build_modify_expr (yyvsp[-2].ttype, yyvsp[-1].code, yyvsp[0].ttype);
		  /* This inhibits warnings in
		     c_common_truthvalue_conversion.  */
		  class = TREE_CODE_CLASS (TREE_CODE (yyval.ttype));
		  if (IS_EXPR_CODE_CLASS (class))
		    C_SET_EXP_ORIGINAL_CODE (yyval.ttype, ERROR_MARK);
		;
    break;}
case 88:
#line 634 "objc-parse.y"
{
		  if (yychar == YYEMPTY)
		    yychar = YYLEX;
		  yyval.ttype = build_external_ref (yyvsp[0].ttype, yychar == '(');
		;
    break;}
case 90:
#line 641 "objc-parse.y"
{ yyval.ttype = fix_string_type (yyval.ttype); ;
    break;}
case 91:
#line 643 "objc-parse.y"
{ yyval.ttype = fname_decl (C_RID_CODE (yyval.ttype), yyval.ttype); ;
    break;}
case 92:
#line 645 "objc-parse.y"
{ start_init (NULL_TREE, NULL, 0);
		  yyvsp[-2].ttype = groktypename (yyvsp[-2].ttype);
		  really_start_incremental_init (yyvsp[-2].ttype); ;
    break;}
case 93:
#line 649 "objc-parse.y"
{ tree constructor = pop_init_level (0);
		  tree type = yyvsp[-5].ttype;
		  finish_init ();

		  if (pedantic && ! flag_isoc99)
		    pedwarn ("ISO C89 forbids compound literals");
		  yyval.ttype = build_compound_literal (type, constructor);
		;
    break;}
case 94:
#line 658 "objc-parse.y"
{ char class = TREE_CODE_CLASS (TREE_CODE (yyvsp[-1].ttype));
		  if (IS_EXPR_CODE_CLASS (class))
		    C_SET_EXP_ORIGINAL_CODE (yyvsp[-1].ttype, ERROR_MARK);
		  yyval.ttype = yyvsp[-1].ttype; ;
    break;}
case 95:
#line 663 "objc-parse.y"
{ yyval.ttype = error_mark_node; ;
    break;}
case 96:
#line 665 "objc-parse.y"
{ tree saved_last_tree;

		   if (pedantic)
		     pedwarn ("ISO C forbids braced-groups within expressions");
		  pop_label_level ();

		  saved_last_tree = COMPOUND_BODY (yyvsp[-2].ttype);
		  RECHAIN_STMTS (yyvsp[-2].ttype, COMPOUND_BODY (yyvsp[-2].ttype));
		  last_tree = saved_last_tree;
		  TREE_CHAIN (last_tree) = NULL_TREE;
		  if (!last_expr_type)
		    last_expr_type = void_type_node;
		  yyval.ttype = build1 (STMT_EXPR, last_expr_type, yyvsp[-2].ttype);
		  TREE_SIDE_EFFECTS (yyval.ttype) = 1;
		;
    break;}
case 97:
#line 681 "objc-parse.y"
{
		  pop_label_level ();
		  last_tree = COMPOUND_BODY (yyvsp[-2].ttype);
		  TREE_CHAIN (last_tree) = NULL_TREE;
		  yyval.ttype = error_mark_node;
		;
    break;}
case 98:
#line 688 "objc-parse.y"
{ yyval.ttype = build_function_call (yyvsp[-3].ttype, yyvsp[-1].ttype); ;
    break;}
case 99:
#line 690 "objc-parse.y"
{ yyval.ttype = build_va_arg (yyvsp[-3].ttype, groktypename (yyvsp[-1].ttype)); ;
    break;}
case 100:
#line 693 "objc-parse.y"
{
                  tree c;

                  c = fold (yyvsp[-5].ttype);
                  STRIP_NOPS (c);
                  if (TREE_CODE (c) != INTEGER_CST)
                    error ("first argument to __builtin_choose_expr not a constant");
                  yyval.ttype = integer_zerop (c) ? yyvsp[-1].ttype : yyvsp[-3].ttype;
		;
    break;}
case 101:
#line 703 "objc-parse.y"
{
		  tree e1, e2;

		  e1 = TYPE_MAIN_VARIANT (groktypename (yyvsp[-3].ttype));
		  e2 = TYPE_MAIN_VARIANT (groktypename (yyvsp[-1].ttype));

		  yyval.ttype = comptypes (e1, e2)
		    ? build_int_2 (1, 0) : build_int_2 (0, 0);
		;
    break;}
case 102:
#line 713 "objc-parse.y"
{ yyval.ttype = build_array_ref (yyvsp[-3].ttype, yyvsp[-1].ttype); ;
    break;}
case 103:
#line 715 "objc-parse.y"
{
		    if (!is_public (yyvsp[-2].ttype, yyvsp[0].ttype))
		      yyval.ttype = error_mark_node;
		    else
		      yyval.ttype = build_component_ref (yyvsp[-2].ttype, yyvsp[0].ttype);
		;
    break;}
case 104:
#line 722 "objc-parse.y"
{
                  tree expr = build_indirect_ref (yyvsp[-2].ttype, "->");

		      if (!is_public (expr, yyvsp[0].ttype))
			yyval.ttype = error_mark_node;
		      else
			yyval.ttype = build_component_ref (expr, yyvsp[0].ttype);
		;
    break;}
case 105:
#line 731 "objc-parse.y"
{ yyval.ttype = build_unary_op (POSTINCREMENT_EXPR, yyvsp[-1].ttype, 0); ;
    break;}
case 106:
#line 733 "objc-parse.y"
{ yyval.ttype = build_unary_op (POSTDECREMENT_EXPR, yyvsp[-1].ttype, 0); ;
    break;}
case 107:
#line 735 "objc-parse.y"
{ yyval.ttype = build_message_expr (yyvsp[0].ttype); ;
    break;}
case 108:
#line 737 "objc-parse.y"
{ yyval.ttype = build_selector_expr (yyvsp[0].ttype); ;
    break;}
case 109:
#line 739 "objc-parse.y"
{ yyval.ttype = build_protocol_expr (yyvsp[0].ttype); ;
    break;}
case 110:
#line 741 "objc-parse.y"
{ yyval.ttype = build_encode_expr (yyvsp[0].ttype); ;
    break;}
case 111:
#line 743 "objc-parse.y"
{ yyval.ttype = build_objc_string_object (yyvsp[0].ttype); ;
    break;}
case 112:
#line 750 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype; ;
    break;}
case 113:
#line 752 "objc-parse.y"
{ yyval.ttype = chainon (yyvsp[-2].ttype, yyvsp[0].ttype); ;
    break;}
case 114:
#line 757 "objc-parse.y"
{
	  parsing_iso_function_signature = false; /* Reset after decls.  */
	;
    break;}
case 115:
#line 764 "objc-parse.y"
{
	  if (warn_traditional && !in_system_header
	      && parsing_iso_function_signature)
	    warning ("traditional C rejects ISO C style function definitions");
	  parsing_iso_function_signature = false; /* Reset after warning.  */
	;
    break;}
case 117:
#line 778 "objc-parse.y"
{ ;
    break;}
case 122:
#line 794 "objc-parse.y"
{ POP_DECLSPEC_STACK; ;
    break;}
case 123:
#line 796 "objc-parse.y"
{ POP_DECLSPEC_STACK; ;
    break;}
case 124:
#line 798 "objc-parse.y"
{ shadow_tag_warned (yyvsp[-1].ttype, 1);
		  pedwarn ("empty declaration"); ;
    break;}
case 125:
#line 801 "objc-parse.y"
{ pedwarn ("empty declaration"); ;
    break;}
case 126:
#line 810 "objc-parse.y"
{ ;
    break;}
case 127:
#line 818 "objc-parse.y"
{ pending_xref_error ();
		  PUSH_DECLSPEC_STACK;
		  split_specs_attrs (yyvsp[0].ttype,
				     &current_declspecs, &prefix_attributes);
		  all_prefix_attributes = prefix_attributes; ;
    break;}
case 128:
#line 829 "objc-parse.y"
{ all_prefix_attributes = chainon (yyvsp[0].ttype, prefix_attributes); ;
    break;}
case 129:
#line 834 "objc-parse.y"
{ POP_DECLSPEC_STACK; ;
    break;}
case 130:
#line 836 "objc-parse.y"
{ POP_DECLSPEC_STACK; ;
    break;}
case 131:
#line 838 "objc-parse.y"
{ POP_DECLSPEC_STACK; ;
    break;}
case 132:
#line 840 "objc-parse.y"
{ POP_DECLSPEC_STACK; ;
    break;}
case 133:
#line 842 "objc-parse.y"
{ shadow_tag (yyvsp[-1].ttype); ;
    break;}
case 134:
#line 844 "objc-parse.y"
{ RESTORE_EXT_FLAGS (yyvsp[-1].ttype); ;
    break;}
case 135:
#line 901 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, NULL_TREE);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 136:
#line 904 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 137:
#line 907 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 138:
#line 913 "objc-parse.y"
{ yyval.ttype = tree_cons (yyvsp[0].ttype, NULL_TREE, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 139:
#line 919 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 140:
#line 922 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 141:
#line 928 "objc-parse.y"
{ yyval.ttype = tree_cons (yyvsp[0].ttype, NULL_TREE, NULL_TREE);
		  TREE_STATIC (yyval.ttype) = 0; ;
    break;}
case 142:
#line 931 "objc-parse.y"
{ yyval.ttype = tree_cons (yyvsp[0].ttype, NULL_TREE, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 143:
#line 937 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, NULL_TREE);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 144:
#line 940 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 145:
#line 943 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 146:
#line 946 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 147:
#line 949 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 148:
#line 952 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 149:
#line 955 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 150:
#line 961 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, NULL_TREE);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 151:
#line 964 "objc-parse.y"
{ yyval.ttype = tree_cons (yyvsp[0].ttype, NULL_TREE, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 152:
#line 967 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 153:
#line 970 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 154:
#line 973 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 155:
#line 976 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 156:
#line 982 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 157:
#line 985 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 158:
#line 988 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 159:
#line 991 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 160:
#line 994 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 161:
#line 997 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 162:
#line 1003 "objc-parse.y"
{ yyval.ttype = tree_cons (yyvsp[0].ttype, NULL_TREE, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 163:
#line 1006 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 164:
#line 1009 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 165:
#line 1012 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 166:
#line 1015 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 167:
#line 1021 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, NULL_TREE);
		  TREE_STATIC (yyval.ttype) = 0; ;
    break;}
case 168:
#line 1024 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 169:
#line 1027 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 170:
#line 1030 "objc-parse.y"
{ if (extra_warnings && TREE_STATIC (yyvsp[-1].ttype))
		    warning ("`%s' is not at beginning of declaration",
			     IDENTIFIER_POINTER (yyvsp[0].ttype));
		  yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 171:
#line 1036 "objc-parse.y"
{ if (extra_warnings && TREE_STATIC (yyvsp[-1].ttype))
		    warning ("`%s' is not at beginning of declaration",
			     IDENTIFIER_POINTER (yyvsp[0].ttype));
		  yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 172:
#line 1042 "objc-parse.y"
{ if (extra_warnings && TREE_STATIC (yyvsp[-1].ttype))
		    warning ("`%s' is not at beginning of declaration",
			     IDENTIFIER_POINTER (yyvsp[0].ttype));
		  yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 173:
#line 1048 "objc-parse.y"
{ if (extra_warnings && TREE_STATIC (yyvsp[-1].ttype))
		    warning ("`%s' is not at beginning of declaration",
			     IDENTIFIER_POINTER (yyvsp[0].ttype));
		  yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 174:
#line 1057 "objc-parse.y"
{ yyval.ttype = tree_cons (yyvsp[0].ttype, NULL_TREE, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 175:
#line 1063 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 176:
#line 1066 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 177:
#line 1069 "objc-parse.y"
{ if (extra_warnings && TREE_STATIC (yyvsp[-1].ttype))
		    warning ("`%s' is not at beginning of declaration",
			     IDENTIFIER_POINTER (yyvsp[0].ttype));
		  yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 178:
#line 1075 "objc-parse.y"
{ if (extra_warnings && TREE_STATIC (yyvsp[-1].ttype))
		    warning ("`%s' is not at beginning of declaration",
			     IDENTIFIER_POINTER (yyvsp[0].ttype));
		  yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 179:
#line 1081 "objc-parse.y"
{ if (extra_warnings && TREE_STATIC (yyvsp[-1].ttype))
		    warning ("`%s' is not at beginning of declaration",
			     IDENTIFIER_POINTER (yyvsp[0].ttype));
		  yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 180:
#line 1087 "objc-parse.y"
{ if (extra_warnings && TREE_STATIC (yyvsp[-1].ttype))
		    warning ("`%s' is not at beginning of declaration",
			     IDENTIFIER_POINTER (yyvsp[0].ttype));
		  yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 181:
#line 1096 "objc-parse.y"
{ yyval.ttype = tree_cons (yyvsp[0].ttype, NULL_TREE, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 182:
#line 1102 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 183:
#line 1105 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 184:
#line 1108 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 185:
#line 1111 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 186:
#line 1114 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 187:
#line 1117 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 188:
#line 1120 "objc-parse.y"
{ if (extra_warnings && TREE_STATIC (yyvsp[-1].ttype))
		    warning ("`%s' is not at beginning of declaration",
			     IDENTIFIER_POINTER (yyvsp[0].ttype));
		  yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 189:
#line 1126 "objc-parse.y"
{ if (extra_warnings && TREE_STATIC (yyvsp[-1].ttype))
		    warning ("`%s' is not at beginning of declaration",
			     IDENTIFIER_POINTER (yyvsp[0].ttype));
		  yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 190:
#line 1132 "objc-parse.y"
{ if (extra_warnings && TREE_STATIC (yyvsp[-1].ttype))
		    warning ("`%s' is not at beginning of declaration",
			     IDENTIFIER_POINTER (yyvsp[0].ttype));
		  yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 191:
#line 1138 "objc-parse.y"
{ if (extra_warnings && TREE_STATIC (yyvsp[-1].ttype))
		    warning ("`%s' is not at beginning of declaration",
			     IDENTIFIER_POINTER (yyvsp[0].ttype));
		  yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 192:
#line 1147 "objc-parse.y"
{ yyval.ttype = tree_cons (yyvsp[0].ttype, NULL_TREE, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 193:
#line 1150 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 194:
#line 1153 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 195:
#line 1156 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 196:
#line 1159 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 197:
#line 1165 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 198:
#line 1168 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 199:
#line 1171 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 200:
#line 1174 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 201:
#line 1177 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 202:
#line 1180 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 203:
#line 1183 "objc-parse.y"
{ if (extra_warnings && TREE_STATIC (yyvsp[-1].ttype))
		    warning ("`%s' is not at beginning of declaration",
			     IDENTIFIER_POINTER (yyvsp[0].ttype));
		  yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 204:
#line 1189 "objc-parse.y"
{ if (extra_warnings && TREE_STATIC (yyvsp[-1].ttype))
		    warning ("`%s' is not at beginning of declaration",
			     IDENTIFIER_POINTER (yyvsp[0].ttype));
		  yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 205:
#line 1195 "objc-parse.y"
{ if (extra_warnings && TREE_STATIC (yyvsp[-1].ttype))
		    warning ("`%s' is not at beginning of declaration",
			     IDENTIFIER_POINTER (yyvsp[0].ttype));
		  yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 206:
#line 1201 "objc-parse.y"
{ if (extra_warnings && TREE_STATIC (yyvsp[-1].ttype))
		    warning ("`%s' is not at beginning of declaration",
			     IDENTIFIER_POINTER (yyvsp[0].ttype));
		  yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 207:
#line 1210 "objc-parse.y"
{ yyval.ttype = tree_cons (yyvsp[0].ttype, NULL_TREE, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = TREE_STATIC (yyvsp[-1].ttype); ;
    break;}
case 208:
#line 1213 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 209:
#line 1216 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 210:
#line 1219 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 211:
#line 1222 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-1].ttype);
		  TREE_STATIC (yyval.ttype) = 1; ;
    break;}
case 268:
#line 1310 "objc-parse.y"
{ yyval.ttype = NULL_TREE; ;
    break;}
case 269:
#line 1312 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype; ;
    break;}
case 273:
#line 1347 "objc-parse.y"
{ OBJC_NEED_RAW_IDENTIFIER (1);	;
    break;}
case 276:
#line 1357 "objc-parse.y"
{ /* For a typedef name, record the meaning, not the name.
		     In case of `foo foo, bar;'.  */
		  yyval.ttype = lookup_name (yyvsp[0].ttype); ;
    break;}
case 277:
#line 1361 "objc-parse.y"
{ yyval.ttype = get_static_reference (yyvsp[-1].ttype, yyvsp[0].ttype); ;
    break;}
case 278:
#line 1363 "objc-parse.y"
{ yyval.ttype = get_object_reference (yyvsp[0].ttype); ;
    break;}
case 279:
#line 1368 "objc-parse.y"
{ yyval.ttype = get_object_reference (yyvsp[0].ttype); ;
    break;}
case 280:
#line 1370 "objc-parse.y"
{ skip_evaluation--; yyval.ttype = TREE_TYPE (yyvsp[-1].ttype); ;
    break;}
case 281:
#line 1372 "objc-parse.y"
{ skip_evaluation--; yyval.ttype = groktypename (yyvsp[-1].ttype); ;
    break;}
case 286:
#line 1389 "objc-parse.y"
{ yyval.ttype = NULL_TREE; ;
    break;}
case 287:
#line 1391 "objc-parse.y"
{ yyval.ttype = yyvsp[-1].ttype; ;
    break;}
case 288:
#line 1396 "objc-parse.y"
{ yyval.ttype = start_decl (yyvsp[-3].ttype, current_declspecs, 1,
					  chainon (yyvsp[-1].ttype, all_prefix_attributes));
		  start_init (yyval.ttype, yyvsp[-2].ttype, global_bindings_p ()); ;
    break;}
case 289:
#line 1401 "objc-parse.y"
{ finish_init ();
		  finish_decl (yyvsp[-1].ttype, yyvsp[0].ttype, yyvsp[-4].ttype); ;
    break;}
case 290:
#line 1404 "objc-parse.y"
{ tree d = start_decl (yyvsp[-2].ttype, current_declspecs, 0,
				       chainon (yyvsp[0].ttype, all_prefix_attributes));
		  finish_decl (d, NULL_TREE, yyvsp[-1].ttype);
                ;
    break;}
case 291:
#line 1412 "objc-parse.y"
{ yyval.ttype = start_decl (yyvsp[-3].ttype, current_declspecs, 1,
					  chainon (yyvsp[-1].ttype, all_prefix_attributes));
		  start_init (yyval.ttype, yyvsp[-2].ttype, global_bindings_p ()); ;
    break;}
case 292:
#line 1417 "objc-parse.y"
{ finish_init ();
		  finish_decl (yyvsp[-1].ttype, yyvsp[0].ttype, yyvsp[-4].ttype); ;
    break;}
case 293:
#line 1420 "objc-parse.y"
{ tree d = start_decl (yyvsp[-2].ttype, current_declspecs, 0,
				       chainon (yyvsp[0].ttype, all_prefix_attributes));
		  finish_decl (d, NULL_TREE, yyvsp[-1].ttype); ;
    break;}
case 294:
#line 1428 "objc-parse.y"
{ yyval.ttype = NULL_TREE; ;
    break;}
case 295:
#line 1430 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype; ;
    break;}
case 296:
#line 1435 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype; ;
    break;}
case 297:
#line 1437 "objc-parse.y"
{ yyval.ttype = chainon (yyvsp[-1].ttype, yyvsp[0].ttype); ;
    break;}
case 298:
#line 1442 "objc-parse.y"
{ yyval.ttype = yyvsp[-2].ttype; ;
    break;}
case 299:
#line 1447 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype; ;
    break;}
case 300:
#line 1449 "objc-parse.y"
{ yyval.ttype = chainon (yyvsp[-2].ttype, yyvsp[0].ttype); ;
    break;}
case 301:
#line 1454 "objc-parse.y"
{ yyval.ttype = NULL_TREE; ;
    break;}
case 302:
#line 1456 "objc-parse.y"
{ yyval.ttype = build_tree_list (yyvsp[0].ttype, NULL_TREE); ;
    break;}
case 303:
#line 1458 "objc-parse.y"
{ yyval.ttype = build_tree_list (yyvsp[-3].ttype, build_tree_list (NULL_TREE, yyvsp[-1].ttype)); ;
    break;}
case 304:
#line 1460 "objc-parse.y"
{ yyval.ttype = build_tree_list (yyvsp[-5].ttype, tree_cons (NULL_TREE, yyvsp[-3].ttype, yyvsp[-1].ttype)); ;
    break;}
case 305:
#line 1462 "objc-parse.y"
{ yyval.ttype = build_tree_list (yyvsp[-3].ttype, yyvsp[-1].ttype); ;
    break;}
case 313:
#line 1485 "objc-parse.y"
{ really_start_incremental_init (NULL_TREE); ;
    break;}
case 314:
#line 1487 "objc-parse.y"
{ yyval.ttype = pop_init_level (0); ;
    break;}
case 315:
#line 1489 "objc-parse.y"
{ yyval.ttype = error_mark_node; ;
    break;}
case 316:
#line 1495 "objc-parse.y"
{ if (pedantic)
		    pedwarn ("ISO C forbids empty initializer braces"); ;
    break;}
case 320:
#line 1509 "objc-parse.y"
{ if (pedantic && ! flag_isoc99)
		    pedwarn ("ISO C89 forbids specifying subobject to initialize"); ;
    break;}
case 321:
#line 1512 "objc-parse.y"
{ if (pedantic)
		    pedwarn ("obsolete use of designated initializer without `='"); ;
    break;}
case 322:
#line 1515 "objc-parse.y"
{ set_init_label (yyvsp[-1].ttype);
		  if (pedantic)
		    pedwarn ("obsolete use of designated initializer with `:'"); ;
    break;}
case 323:
#line 1519 "objc-parse.y"
{;
    break;}
case 325:
#line 1525 "objc-parse.y"
{ push_init_level (0); ;
    break;}
case 326:
#line 1527 "objc-parse.y"
{ process_init_element (pop_init_level (0)); ;
    break;}
case 327:
#line 1529 "objc-parse.y"
{ process_init_element (yyvsp[0].ttype); ;
    break;}
case 331:
#line 1540 "objc-parse.y"
{ set_init_label (yyvsp[0].ttype); ;
    break;}
case 332:
#line 1542 "objc-parse.y"
{ set_init_index (yyvsp[-3].ttype, yyvsp[-1].ttype);
		  if (pedantic)
		    pedwarn ("ISO C forbids specifying range of elements to initialize"); ;
    break;}
case 333:
#line 1546 "objc-parse.y"
{ set_init_index (yyvsp[-1].ttype, NULL_TREE); ;
    break;}
case 334:
#line 1551 "objc-parse.y"
{ if (pedantic)
		    pedwarn ("ISO C forbids nested functions");

		  push_function_context ();
		  if (! start_function (current_declspecs, yyvsp[0].ttype,
					all_prefix_attributes))
		    {
		      pop_function_context ();
		      YYERROR1;
		    }
		  parsing_iso_function_signature = false; /* Don't warn about nested functions.  */
		;
    break;}
case 335:
#line 1564 "objc-parse.y"
{ store_parm_decls (); ;
    break;}
case 336:
#line 1572 "objc-parse.y"
{ tree decl = current_function_decl;
		  DECL_SOURCE_FILE (decl) = yyvsp[-2].filename;
		  DECL_SOURCE_LINE (decl) = yyvsp[-1].lineno;
		  finish_function (1, 1);
		  pop_function_context ();
		  add_decl_stmt (decl); ;
    break;}
case 337:
#line 1582 "objc-parse.y"
{ if (pedantic)
		    pedwarn ("ISO C forbids nested functions");

		  push_function_context ();
		  if (! start_function (current_declspecs, yyvsp[0].ttype,
					all_prefix_attributes))
		    {
		      pop_function_context ();
		      YYERROR1;
		    }
		  parsing_iso_function_signature = false; /* Don't warn about nested functions.  */
		;
    break;}
case 338:
#line 1595 "objc-parse.y"
{ store_parm_decls (); ;
    break;}
case 339:
#line 1603 "objc-parse.y"
{ tree decl = current_function_decl;
		  DECL_SOURCE_FILE (decl) = yyvsp[-2].filename;
		  DECL_SOURCE_LINE (decl) = yyvsp[-1].lineno;
		  finish_function (1, 1);
		  pop_function_context ();
		  add_decl_stmt (decl); ;
    break;}
case 342:
#line 1623 "objc-parse.y"
{ yyval.ttype = yyvsp[-2].ttype ? tree_cons (yyvsp[-2].ttype, yyvsp[-1].ttype, NULL_TREE) : yyvsp[-1].ttype; ;
    break;}
case 343:
#line 1625 "objc-parse.y"
{ yyval.ttype = build_nt (CALL_EXPR, yyvsp[-2].ttype, yyvsp[0].ttype, NULL_TREE); ;
    break;}
case 344:
#line 1630 "objc-parse.y"
{ yyval.ttype = set_array_declarator_type (yyvsp[0].ttype, yyvsp[-1].ttype, 0); ;
    break;}
case 345:
#line 1632 "objc-parse.y"
{ yyval.ttype = make_pointer_declarator (yyvsp[-1].ttype, yyvsp[0].ttype); ;
    break;}
case 350:
#line 1648 "objc-parse.y"
{ yyval.ttype = build_nt (CALL_EXPR, yyvsp[-2].ttype, yyvsp[0].ttype, NULL_TREE); ;
    break;}
case 351:
#line 1653 "objc-parse.y"
{ yyval.ttype = set_array_declarator_type (yyvsp[0].ttype, yyvsp[-1].ttype, 0); ;
    break;}
case 354:
#line 1660 "objc-parse.y"
{ yyval.ttype = build_nt (CALL_EXPR, yyvsp[-2].ttype, yyvsp[0].ttype, NULL_TREE); ;
    break;}
case 355:
#line 1665 "objc-parse.y"
{ yyval.ttype = set_array_declarator_type (yyvsp[0].ttype, yyvsp[-1].ttype, 0); ;
    break;}
case 356:
#line 1667 "objc-parse.y"
{ yyval.ttype = make_pointer_declarator (yyvsp[-1].ttype, yyvsp[0].ttype); ;
    break;}
case 357:
#line 1669 "objc-parse.y"
{ yyval.ttype = make_pointer_declarator (yyvsp[-1].ttype, yyvsp[0].ttype); ;
    break;}
case 358:
#line 1671 "objc-parse.y"
{ yyval.ttype = yyvsp[-2].ttype ? tree_cons (yyvsp[-2].ttype, yyvsp[-1].ttype, NULL_TREE) : yyvsp[-1].ttype; ;
    break;}
case 359:
#line 1679 "objc-parse.y"
{ yyval.ttype = build_nt (CALL_EXPR, yyvsp[-2].ttype, yyvsp[0].ttype, NULL_TREE); ;
    break;}
case 360:
#line 1684 "objc-parse.y"
{ yyval.ttype = yyvsp[-2].ttype ? tree_cons (yyvsp[-2].ttype, yyvsp[-1].ttype, NULL_TREE) : yyvsp[-1].ttype; ;
    break;}
case 361:
#line 1686 "objc-parse.y"
{ yyval.ttype = make_pointer_declarator (yyvsp[-1].ttype, yyvsp[0].ttype); ;
    break;}
case 362:
#line 1688 "objc-parse.y"
{ yyval.ttype = set_array_declarator_type (yyvsp[0].ttype, yyvsp[-1].ttype, 0); ;
    break;}
case 364:
#line 1694 "objc-parse.y"
{ yyval.ttype = NULL_TREE; ;
    break;}
case 365:
#line 1696 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype; ;
    break;}
case 366:
#line 1701 "objc-parse.y"
{ yyval.ttype = NULL_TREE; ;
    break;}
case 367:
#line 1703 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype; ;
    break;}
case 368:
#line 1708 "objc-parse.y"
{ yyval.ttype = NULL_TREE; ;
    break;}
case 369:
#line 1710 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype; ;
    break;}
case 370:
#line 1721 "objc-parse.y"
{ yyval.ttype = start_struct (RECORD_TYPE, yyvsp[-1].ttype);
		  /* Start scope of tag before parsing components.  */
		;
    break;}
case 371:
#line 1725 "objc-parse.y"
{ yyval.ttype = finish_struct (yyvsp[-3].ttype, yyvsp[-2].ttype, chainon (yyvsp[-6].ttype, yyvsp[0].ttype)); ;
    break;}
case 372:
#line 1727 "objc-parse.y"
{ yyval.ttype = finish_struct (start_struct (RECORD_TYPE, NULL_TREE),
				      yyvsp[-2].ttype, chainon (yyvsp[-4].ttype, yyvsp[0].ttype));
		;
    break;}
case 373:
#line 1731 "objc-parse.y"
{ yyval.ttype = start_struct (UNION_TYPE, yyvsp[-1].ttype); ;
    break;}
case 374:
#line 1733 "objc-parse.y"
{ yyval.ttype = finish_struct (yyvsp[-3].ttype, yyvsp[-2].ttype, chainon (yyvsp[-6].ttype, yyvsp[0].ttype)); ;
    break;}
case 375:
#line 1735 "objc-parse.y"
{ yyval.ttype = finish_struct (start_struct (UNION_TYPE, NULL_TREE),
				      yyvsp[-2].ttype, chainon (yyvsp[-4].ttype, yyvsp[0].ttype));
		;
    break;}
case 376:
#line 1739 "objc-parse.y"
{ yyval.ttype = start_enum (yyvsp[-1].ttype); ;
    break;}
case 377:
#line 1741 "objc-parse.y"
{ yyval.ttype = finish_enum (yyvsp[-4].ttype, nreverse (yyvsp[-3].ttype),
				    chainon (yyvsp[-7].ttype, yyvsp[0].ttype)); ;
    break;}
case 378:
#line 1744 "objc-parse.y"
{ yyval.ttype = start_enum (NULL_TREE); ;
    break;}
case 379:
#line 1746 "objc-parse.y"
{ yyval.ttype = finish_enum (yyvsp[-4].ttype, nreverse (yyvsp[-3].ttype),
				    chainon (yyvsp[-6].ttype, yyvsp[0].ttype)); ;
    break;}
case 380:
#line 1752 "objc-parse.y"
{ yyval.ttype = xref_tag (RECORD_TYPE, yyvsp[0].ttype); ;
    break;}
case 381:
#line 1754 "objc-parse.y"
{ yyval.ttype = xref_tag (UNION_TYPE, yyvsp[0].ttype); ;
    break;}
case 382:
#line 1756 "objc-parse.y"
{ yyval.ttype = xref_tag (ENUMERAL_TYPE, yyvsp[0].ttype);
		  /* In ISO C, enumerated types can be referred to
		     only if already defined.  */
		  if (pedantic && !COMPLETE_TYPE_P (yyval.ttype))
		    pedwarn ("ISO C forbids forward references to `enum' types"); ;
    break;}
case 386:
#line 1771 "objc-parse.y"
{ if (pedantic && ! flag_isoc99)
		    pedwarn ("comma at end of enumerator list"); ;
    break;}
case 387:
#line 1777 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype; ;
    break;}
case 388:
#line 1779 "objc-parse.y"
{ yyval.ttype = chainon (yyvsp[-1].ttype, yyvsp[0].ttype);
		  pedwarn ("no semicolon at end of struct or union"); ;
    break;}
case 389:
#line 1784 "objc-parse.y"
{ yyval.ttype = NULL_TREE; ;
    break;}
case 390:
#line 1786 "objc-parse.y"
{ yyval.ttype = chainon (yyvsp[-2].ttype, yyvsp[-1].ttype); ;
    break;}
case 391:
#line 1788 "objc-parse.y"
{ if (pedantic)
		    pedwarn ("extra semicolon in struct or union specified"); ;
    break;}
case 392:
#line 1792 "objc-parse.y"
{
		  tree interface = lookup_interface (yyvsp[-1].ttype);

		  if (interface)
		    yyval.ttype = get_class_ivars (interface);
		  else
		    {
		      error ("cannot find interface declaration for `%s'",
			     IDENTIFIER_POINTER (yyvsp[-1].ttype));
		      yyval.ttype = NULL_TREE;
		    }
		;
    break;}
case 393:
#line 1808 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype;
		  POP_DECLSPEC_STACK; ;
    break;}
case 394:
#line 1811 "objc-parse.y"
{
		  /* Support for unnamed structs or unions as members of
		     structs or unions (which is [a] useful and [b] supports
		     MS P-SDK).  */
		  if (pedantic)
		    pedwarn ("ISO C doesn't support unnamed structs/unions");

		  yyval.ttype = grokfield(yyvsp[-1].filename, yyvsp[0].lineno, NULL, current_declspecs, NULL_TREE);
		  POP_DECLSPEC_STACK; ;
    break;}
case 395:
#line 1821 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype;
		  POP_DECLSPEC_STACK; ;
    break;}
case 396:
#line 1824 "objc-parse.y"
{ if (pedantic)
		    pedwarn ("ISO C forbids member declarations with no members");
		  shadow_tag(yyvsp[0].ttype);
		  yyval.ttype = NULL_TREE; ;
    break;}
case 397:
#line 1829 "objc-parse.y"
{ yyval.ttype = NULL_TREE; ;
    break;}
case 398:
#line 1831 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype;
		  RESTORE_EXT_FLAGS (yyvsp[-1].ttype); ;
    break;}
case 400:
#line 1838 "objc-parse.y"
{ yyval.ttype = chainon (yyvsp[-3].ttype, yyvsp[0].ttype); ;
    break;}
case 402:
#line 1844 "objc-parse.y"
{ yyval.ttype = chainon (yyvsp[-3].ttype, yyvsp[0].ttype); ;
    break;}
case 403:
#line 1849 "objc-parse.y"
{ yyval.ttype = grokfield (yyvsp[-3].filename, yyvsp[-2].lineno, yyvsp[-1].ttype, current_declspecs, NULL_TREE);
		  decl_attributes (&yyval.ttype, chainon (yyvsp[0].ttype, all_prefix_attributes), 0); ;
    break;}
case 404:
#line 1853 "objc-parse.y"
{ yyval.ttype = grokfield (yyvsp[-5].filename, yyvsp[-4].lineno, yyvsp[-3].ttype, current_declspecs, yyvsp[-1].ttype);
		  decl_attributes (&yyval.ttype, chainon (yyvsp[0].ttype, all_prefix_attributes), 0); ;
    break;}
case 405:
#line 1856 "objc-parse.y"
{ yyval.ttype = grokfield (yyvsp[-4].filename, yyvsp[-3].lineno, NULL_TREE, current_declspecs, yyvsp[-1].ttype);
		  decl_attributes (&yyval.ttype, chainon (yyvsp[0].ttype, all_prefix_attributes), 0); ;
    break;}
case 406:
#line 1862 "objc-parse.y"
{ yyval.ttype = grokfield (yyvsp[-3].filename, yyvsp[-2].lineno, yyvsp[-1].ttype, current_declspecs, NULL_TREE);
		  decl_attributes (&yyval.ttype, chainon (yyvsp[0].ttype, all_prefix_attributes), 0); ;
    break;}
case 407:
#line 1866 "objc-parse.y"
{ yyval.ttype = grokfield (yyvsp[-5].filename, yyvsp[-4].lineno, yyvsp[-3].ttype, current_declspecs, yyvsp[-1].ttype);
		  decl_attributes (&yyval.ttype, chainon (yyvsp[0].ttype, all_prefix_attributes), 0); ;
    break;}
case 408:
#line 1869 "objc-parse.y"
{ yyval.ttype = grokfield (yyvsp[-4].filename, yyvsp[-3].lineno, NULL_TREE, current_declspecs, yyvsp[-1].ttype);
		  decl_attributes (&yyval.ttype, chainon (yyvsp[0].ttype, all_prefix_attributes), 0); ;
    break;}
case 410:
#line 1881 "objc-parse.y"
{ if (yyvsp[-2].ttype == error_mark_node)
		    yyval.ttype = yyvsp[-2].ttype;
		  else
		    yyval.ttype = chainon (yyvsp[0].ttype, yyvsp[-2].ttype); ;
    break;}
case 411:
#line 1886 "objc-parse.y"
{ yyval.ttype = error_mark_node; ;
    break;}
case 412:
#line 1892 "objc-parse.y"
{ yyval.ttype = build_enumerator (yyvsp[0].ttype, NULL_TREE); ;
    break;}
case 413:
#line 1894 "objc-parse.y"
{ yyval.ttype = build_enumerator (yyvsp[-2].ttype, yyvsp[0].ttype); ;
    break;}
case 414:
#line 1899 "objc-parse.y"
{ pending_xref_error ();
		  yyval.ttype = yyvsp[0].ttype; ;
    break;}
case 415:
#line 1902 "objc-parse.y"
{ yyval.ttype = build_tree_list (yyvsp[-1].ttype, yyvsp[0].ttype); ;
    break;}
case 416:
#line 1907 "objc-parse.y"
{ yyval.ttype = NULL_TREE; ;
    break;}
case 418:
#line 1913 "objc-parse.y"
{ yyval.ttype = build_tree_list (build_tree_list (current_declspecs,
							 NULL_TREE),
					all_prefix_attributes); ;
    break;}
case 419:
#line 1917 "objc-parse.y"
{ yyval.ttype = build_tree_list (build_tree_list (current_declspecs,
							 yyvsp[0].ttype),
					all_prefix_attributes); ;
    break;}
case 420:
#line 1921 "objc-parse.y"
{ yyval.ttype = build_tree_list (build_tree_list (current_declspecs,
							 yyvsp[-1].ttype),
					chainon (yyvsp[0].ttype, all_prefix_attributes)); ;
    break;}
case 424:
#line 1934 "objc-parse.y"
{ yyval.ttype = make_pointer_declarator (yyvsp[-1].ttype, yyvsp[0].ttype); ;
    break;}
case 425:
#line 1939 "objc-parse.y"
{ yyval.ttype = make_pointer_declarator (yyvsp[0].ttype, NULL_TREE); ;
    break;}
case 426:
#line 1941 "objc-parse.y"
{ yyval.ttype = make_pointer_declarator (yyvsp[-1].ttype, yyvsp[0].ttype); ;
    break;}
case 427:
#line 1946 "objc-parse.y"
{ yyval.ttype = yyvsp[-2].ttype ? tree_cons (yyvsp[-2].ttype, yyvsp[-1].ttype, NULL_TREE) : yyvsp[-1].ttype; ;
    break;}
case 428:
#line 1948 "objc-parse.y"
{ yyval.ttype = build_nt (CALL_EXPR, yyvsp[-2].ttype, yyvsp[0].ttype, NULL_TREE); ;
    break;}
case 429:
#line 1950 "objc-parse.y"
{ yyval.ttype = set_array_declarator_type (yyvsp[0].ttype, yyvsp[-1].ttype, 1); ;
    break;}
case 430:
#line 1952 "objc-parse.y"
{ yyval.ttype = build_nt (CALL_EXPR, NULL_TREE, yyvsp[0].ttype, NULL_TREE); ;
    break;}
case 431:
#line 1954 "objc-parse.y"
{ yyval.ttype = set_array_declarator_type (yyvsp[0].ttype, NULL_TREE, 1); ;
    break;}
case 432:
#line 1961 "objc-parse.y"
{ yyval.ttype = build_array_declarator (yyvsp[-1].ttype, yyvsp[-2].ttype, 0, 0); ;
    break;}
case 433:
#line 1963 "objc-parse.y"
{ yyval.ttype = build_array_declarator (NULL_TREE, yyvsp[-1].ttype, 0, 0); ;
    break;}
case 434:
#line 1965 "objc-parse.y"
{ yyval.ttype = build_array_declarator (NULL_TREE, yyvsp[-2].ttype, 0, 1); ;
    break;}
case 435:
#line 1967 "objc-parse.y"
{ yyval.ttype = build_array_declarator (yyvsp[-1].ttype, yyvsp[-2].ttype, 1, 0); ;
    break;}
case 436:
#line 1970 "objc-parse.y"
{ yyval.ttype = build_array_declarator (yyvsp[-1].ttype, yyvsp[-3].ttype, 1, 0); ;
    break;}
case 439:
#line 1983 "objc-parse.y"
{
		  pedwarn ("deprecated use of label at end of compound statement");
		;
    break;}
case 447:
#line 2000 "objc-parse.y"
{ if (pedantic && !flag_isoc99)
		    pedwarn ("ISO C89 forbids mixed declarations and code"); ;
    break;}
case 462:
#line 2030 "objc-parse.y"
{ pushlevel (0);
		  clear_last_expr ();
		  add_scope_stmt (/*begin_p=*/1, /*partial_p=*/0);
		  if (objc_method_context)
		    add_objc_decls ();
		;
    break;}
case 463:
#line 2039 "objc-parse.y"
{ yyval.ttype = add_scope_stmt (/*begin_p=*/0, /*partial_p=*/0); ;
    break;}
case 464:
#line 2044 "objc-parse.y"
{ if (flag_isoc99)
		    {
		      yyval.ttype = c_begin_compound_stmt ();
		      pushlevel (0);
		      clear_last_expr ();
		      add_scope_stmt (/*begin_p=*/1, /*partial_p=*/0);
		      if (objc_method_context)
			add_objc_decls ();
		    }
		  else
		    yyval.ttype = NULL_TREE;
		;
    break;}
case 465:
#line 2062 "objc-parse.y"
{ if (flag_isoc99)
		    {
		      tree scope_stmt = add_scope_stmt (/*begin_p=*/0, /*partial_p=*/0);
		      yyval.ttype = poplevel (kept_level_p (), 0, 0);
		      SCOPE_STMT_BLOCK (TREE_PURPOSE (scope_stmt))
			= SCOPE_STMT_BLOCK (TREE_VALUE (scope_stmt))
			= yyval.ttype;
		    }
		  else
		    yyval.ttype = NULL_TREE; ;
    break;}
case 467:
#line 2079 "objc-parse.y"
{ if (pedantic)
		    pedwarn ("ISO C forbids label declarations"); ;
    break;}
case 470:
#line 2090 "objc-parse.y"
{ tree link;
		  for (link = yyvsp[-1].ttype; link; link = TREE_CHAIN (link))
		    {
		      tree label = shadow_label (TREE_VALUE (link));
		      C_DECLARED_LABEL_FLAG (label) = 1;
		      add_decl_stmt (label);
		    }
		;
    break;}
case 471:
#line 2104 "objc-parse.y"
{;
    break;}
case 473:
#line 2108 "objc-parse.y"
{ compstmt_count++;
                      yyval.ttype = c_begin_compound_stmt (); ;
    break;}
case 474:
#line 2113 "objc-parse.y"
{ yyval.ttype = convert (void_type_node, integer_zero_node); ;
    break;}
case 475:
#line 2115 "objc-parse.y"
{ yyval.ttype = poplevel (kept_level_p (), 1, 0);
		  SCOPE_STMT_BLOCK (TREE_PURPOSE (yyvsp[0].ttype))
		    = SCOPE_STMT_BLOCK (TREE_VALUE (yyvsp[0].ttype))
		    = yyval.ttype; ;
    break;}
case 478:
#line 2128 "objc-parse.y"
{ if (current_function_decl == 0)
		    {
		      error ("braced-group within expression allowed only inside a function");
		      YYERROR;
		    }
		  /* We must force a BLOCK for this level
		     so that, if it is not expanded later,
		     there is a way to turn off the entire subtree of blocks
		     that are contained in it.  */
		  keep_next_level ();
		  push_label_level ();
		  compstmt_count++;
		  yyval.ttype = add_stmt (build_stmt (COMPOUND_STMT, last_tree));
		;
    break;}
case 479:
#line 2145 "objc-parse.y"
{ RECHAIN_STMTS (yyvsp[-1].ttype, COMPOUND_BODY (yyvsp[-1].ttype));
		  last_expr_type = NULL_TREE;
                  yyval.ttype = yyvsp[-1].ttype; ;
    break;}
case 480:
#line 2153 "objc-parse.y"
{ c_finish_then (); ;
    break;}
case 482:
#line 2170 "objc-parse.y"
{ yyval.ttype = c_begin_if_stmt (); ;
    break;}
case 483:
#line 2172 "objc-parse.y"
{ c_expand_start_cond (c_common_truthvalue_conversion (yyvsp[-1].ttype),
				       compstmt_count,yyvsp[-3].ttype);
		  yyval.itype = stmt_count;
		  if_stmt_file = yyvsp[-7].filename;
		  if_stmt_line = yyvsp[-6].lineno; ;
    break;}
case 484:
#line 2184 "objc-parse.y"
{ stmt_count++;
		  compstmt_count++;
		  yyval.ttype
		    = add_stmt (build_stmt (DO_STMT, NULL_TREE,
					    NULL_TREE));
		  /* In the event that a parse error prevents
		     parsing the complete do-statement, set the
		     condition now.  Otherwise, we can get crashes at
		     RTL-generation time.  */
		  DO_COND (yyval.ttype) = error_mark_node; ;
    break;}
case 485:
#line 2195 "objc-parse.y"
{ yyval.ttype = yyvsp[-2].ttype;
		  RECHAIN_STMTS (yyval.ttype, DO_BODY (yyval.ttype)); ;
    break;}
case 486:
#line 2203 "objc-parse.y"
{ if (yychar == YYEMPTY)
		    yychar = YYLEX;
		  yyval.filename = input_filename; ;
    break;}
case 487:
#line 2209 "objc-parse.y"
{ if (yychar == YYEMPTY)
		    yychar = YYLEX;
		  yyval.lineno = lineno; ;
    break;}
case 490:
#line 2222 "objc-parse.y"
{ if (flag_isoc99)
		    RECHAIN_STMTS (yyvsp[-2].ttype, COMPOUND_BODY (yyvsp[-2].ttype)); ;
    break;}
case 491:
#line 2228 "objc-parse.y"
{ if (yyvsp[0].ttype)
		    {
		      STMT_LINENO (yyvsp[0].ttype) = yyvsp[-1].lineno;
		      /* ??? We currently have no way of recording
			 the filename for a statement.  This probably
			 matters little in practice at the moment,
			 but I suspect that problems will occur when
			 doing inlining at the tree level.  */
		    }
		;
    break;}
case 492:
#line 2242 "objc-parse.y"
{ if (yyvsp[0].ttype)
		    {
		      STMT_LINENO (yyvsp[0].ttype) = yyvsp[-1].lineno;
		    }
		;
    break;}
case 493:
#line 2251 "objc-parse.y"
{ c_expand_start_else ();
		  yyvsp[-1].itype = stmt_count; ;
    break;}
case 494:
#line 2254 "objc-parse.y"
{ c_finish_else ();
		  c_expand_end_cond ();
		  if (extra_warnings && stmt_count == yyvsp[-3].itype)
		    warning ("empty body in an else-statement"); ;
    break;}
case 495:
#line 2259 "objc-parse.y"
{ c_expand_end_cond ();
		  /* This warning is here instead of in simple_if, because we
		     do not want a warning if an empty if is followed by an
		     else statement.  Increment stmt_count so we don't
		     give a second error if this is a nested `if'.  */
		  if (extra_warnings && stmt_count++ == yyvsp[0].itype)
		    warning_with_file_and_line (if_stmt_file, if_stmt_line,
						"empty body in an if-statement"); ;
    break;}
case 496:
#line 2271 "objc-parse.y"
{ c_expand_end_cond (); ;
    break;}
case 497:
#line 2281 "objc-parse.y"
{ stmt_count++;
		  yyval.ttype = c_begin_while_stmt (); ;
    break;}
case 498:
#line 2284 "objc-parse.y"
{ yyvsp[-1].ttype = c_common_truthvalue_conversion (yyvsp[-1].ttype);
		  c_finish_while_stmt_cond
		    (c_common_truthvalue_conversion (yyvsp[-1].ttype), yyvsp[-3].ttype);
		  yyval.ttype = add_stmt (yyvsp[-3].ttype); ;
    break;}
case 499:
#line 2289 "objc-parse.y"
{ RECHAIN_STMTS (yyvsp[-1].ttype, WHILE_BODY (yyvsp[-1].ttype)); ;
    break;}
case 500:
#line 2292 "objc-parse.y"
{ DO_COND (yyvsp[-4].ttype) = c_common_truthvalue_conversion (yyvsp[-2].ttype); ;
    break;}
case 501:
#line 2294 "objc-parse.y"
{ ;
    break;}
case 502:
#line 2296 "objc-parse.y"
{ yyval.ttype = build_stmt (FOR_STMT, NULL_TREE, NULL_TREE,
					  NULL_TREE, NULL_TREE);
		  add_stmt (yyval.ttype); ;
    break;}
case 503:
#line 2300 "objc-parse.y"
{ stmt_count++;
		  RECHAIN_STMTS (yyvsp[-2].ttype, FOR_INIT_STMT (yyvsp[-2].ttype)); ;
    break;}
case 504:
#line 2303 "objc-parse.y"
{ if (yyvsp[-1].ttype)
		    FOR_COND (yyvsp[-5].ttype)
		      = c_common_truthvalue_conversion (yyvsp[-1].ttype); ;
    break;}
case 505:
#line 2307 "objc-parse.y"
{ FOR_EXPR (yyvsp[-8].ttype) = yyvsp[-1].ttype; ;
    break;}
case 506:
#line 2309 "objc-parse.y"
{ RECHAIN_STMTS (yyvsp[-10].ttype, FOR_BODY (yyvsp[-10].ttype)); ;
    break;}
case 507:
#line 2311 "objc-parse.y"
{ stmt_count++;
		  yyval.ttype = c_start_case (yyvsp[-1].ttype); ;
    break;}
case 508:
#line 2314 "objc-parse.y"
{ c_finish_case (); ;
    break;}
case 509:
#line 2319 "objc-parse.y"
{ add_stmt (build_stmt (EXPR_STMT, yyvsp[-1].ttype)); ;
    break;}
case 510:
#line 2321 "objc-parse.y"
{ check_for_loop_decls (); ;
    break;}
case 511:
#line 2327 "objc-parse.y"
{ stmt_count++; yyval.ttype = yyvsp[0].ttype; ;
    break;}
case 512:
#line 2329 "objc-parse.y"
{ stmt_count++;
		  yyval.ttype = c_expand_expr_stmt (yyvsp[-1].ttype); ;
    break;}
case 513:
#line 2332 "objc-parse.y"
{ if (flag_isoc99)
		    RECHAIN_STMTS (yyvsp[-2].ttype, COMPOUND_BODY (yyvsp[-2].ttype));
		  yyval.ttype = NULL_TREE; ;
    break;}
case 514:
#line 2336 "objc-parse.y"
{ stmt_count++;
		  yyval.ttype = add_stmt (build_break_stmt ()); ;
    break;}
case 515:
#line 2339 "objc-parse.y"
{ stmt_count++;
		  yyval.ttype = add_stmt (build_continue_stmt ()); ;
    break;}
case 516:
#line 2342 "objc-parse.y"
{ stmt_count++;
		  yyval.ttype = c_expand_return (NULL_TREE); ;
    break;}
case 517:
#line 2345 "objc-parse.y"
{ stmt_count++;
		  yyval.ttype = c_expand_return (yyvsp[-1].ttype); ;
    break;}
case 518:
#line 2348 "objc-parse.y"
{ stmt_count++;
		  yyval.ttype = simple_asm_stmt (yyvsp[-2].ttype); ;
    break;}
case 519:
#line 2352 "objc-parse.y"
{ stmt_count++;
		  yyval.ttype = build_asm_stmt (yyvsp[-6].ttype, yyvsp[-4].ttype, yyvsp[-2].ttype, NULL_TREE, NULL_TREE); ;
    break;}
case 520:
#line 2357 "objc-parse.y"
{ stmt_count++;
		  yyval.ttype = build_asm_stmt (yyvsp[-8].ttype, yyvsp[-6].ttype, yyvsp[-4].ttype, yyvsp[-2].ttype, NULL_TREE); ;
    break;}
case 521:
#line 2362 "objc-parse.y"
{ stmt_count++;
		  yyval.ttype = build_asm_stmt (yyvsp[-10].ttype, yyvsp[-8].ttype, yyvsp[-6].ttype, yyvsp[-4].ttype, yyvsp[-2].ttype); ;
    break;}
case 522:
#line 2365 "objc-parse.y"
{ tree decl;
		  stmt_count++;
		  decl = lookup_label (yyvsp[-1].ttype);
		  if (decl != 0)
		    {
		      TREE_USED (decl) = 1;
		      yyval.ttype = add_stmt (build_stmt (GOTO_STMT, decl));
		    }
		  else
		    yyval.ttype = NULL_TREE;
		;
    break;}
case 523:
#line 2377 "objc-parse.y"
{ if (pedantic)
		    pedwarn ("ISO C forbids `goto *expr;'");
		  stmt_count++;
		  yyvsp[-1].ttype = convert (ptr_type_node, yyvsp[-1].ttype);
		  yyval.ttype = add_stmt (build_stmt (GOTO_STMT, yyvsp[-1].ttype)); ;
    break;}
case 524:
#line 2383 "objc-parse.y"
{ yyval.ttype = NULL_TREE; ;
    break;}
case 525:
#line 2391 "objc-parse.y"
{ stmt_count++;
		  yyval.ttype = do_case (yyvsp[-1].ttype, NULL_TREE); ;
    break;}
case 526:
#line 2394 "objc-parse.y"
{ stmt_count++;
		  yyval.ttype = do_case (yyvsp[-3].ttype, yyvsp[-1].ttype); ;
    break;}
case 527:
#line 2397 "objc-parse.y"
{ stmt_count++;
		  yyval.ttype = do_case (NULL_TREE, NULL_TREE); ;
    break;}
case 528:
#line 2400 "objc-parse.y"
{ tree label = define_label (yyvsp[-3].filename, yyvsp[-2].lineno, yyvsp[-4].ttype);
		  stmt_count++;
		  if (label)
		    {
		      decl_attributes (&label, yyvsp[0].ttype, 0);
		      yyval.ttype = add_stmt (build_stmt (LABEL_STMT, label));
		    }
		  else
		    yyval.ttype = NULL_TREE;
		;
    break;}
case 529:
#line 2416 "objc-parse.y"
{ emit_line_note (input_filename, lineno);
		  yyval.ttype = NULL_TREE; ;
    break;}
case 530:
#line 2419 "objc-parse.y"
{ emit_line_note (input_filename, lineno); ;
    break;}
case 531:
#line 2424 "objc-parse.y"
{ yyval.ttype = NULL_TREE; ;
    break;}
case 533:
#line 2431 "objc-parse.y"
{ yyval.ttype = NULL_TREE; ;
    break;}
case 536:
#line 2438 "objc-parse.y"
{ yyval.ttype = chainon (yyvsp[-2].ttype, yyvsp[0].ttype); ;
    break;}
case 537:
#line 2443 "objc-parse.y"
{ yyval.ttype = build_tree_list (build_tree_list (NULL_TREE, yyvsp[-3].ttype), yyvsp[-1].ttype); ;
    break;}
case 538:
#line 2445 "objc-parse.y"
{ yyvsp[-5].ttype = build_string (IDENTIFIER_LENGTH (yyvsp[-5].ttype),
				     IDENTIFIER_POINTER (yyvsp[-5].ttype));
		  yyval.ttype = build_tree_list (build_tree_list (yyvsp[-5].ttype, yyvsp[-3].ttype), yyvsp[-1].ttype); ;
    break;}
case 539:
#line 2452 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, NULL_TREE); ;
    break;}
case 540:
#line 2454 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, yyvsp[0].ttype, yyvsp[-2].ttype); ;
    break;}
case 541:
#line 2464 "objc-parse.y"
{ pushlevel (0);
		  clear_parm_order ();
		  declare_parm_level (0); ;
    break;}
case 542:
#line 2468 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype;
		  parmlist_tags_warning ();
		  poplevel (0, 0, 0); ;
    break;}
case 544:
#line 2476 "objc-parse.y"
{ tree parm;
		  if (pedantic)
		    pedwarn ("ISO C forbids forward parameter declarations");
		  /* Mark the forward decls as such.  */
		  for (parm = getdecls (); parm; parm = TREE_CHAIN (parm))
		    TREE_ASM_WRITTEN (parm) = 1;
		  clear_parm_order (); ;
    break;}
case 545:
#line 2484 "objc-parse.y"
{ /* Dummy action so attributes are in known place
		     on parser stack.  */ ;
    break;}
case 546:
#line 2487 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype; ;
    break;}
case 547:
#line 2489 "objc-parse.y"
{ yyval.ttype = tree_cons (NULL_TREE, NULL_TREE, NULL_TREE); ;
    break;}
case 548:
#line 2495 "objc-parse.y"
{ yyval.ttype = get_parm_info (0); ;
    break;}
case 549:
#line 2497 "objc-parse.y"
{ yyval.ttype = get_parm_info (0);
		  /* Gcc used to allow this as an extension.  However, it does
		     not work for all targets, and thus has been disabled.
		     Also, since func (...) and func () are indistinguishable,
		     it caused problems with the code in expand_builtin which
		     tries to verify that BUILT_IN_NEXT_ARG is being used
		     correctly.  */
		  error ("ISO C requires a named argument before `...'");
		;
    break;}
case 550:
#line 2507 "objc-parse.y"
{ yyval.ttype = get_parm_info (1);
		  parsing_iso_function_signature = true;
		;
    break;}
case 551:
#line 2511 "objc-parse.y"
{ yyval.ttype = get_parm_info (0); ;
    break;}
case 552:
#line 2516 "objc-parse.y"
{ push_parm_decl (yyvsp[0].ttype); ;
    break;}
case 553:
#line 2518 "objc-parse.y"
{ push_parm_decl (yyvsp[0].ttype); ;
    break;}
case 554:
#line 2525 "objc-parse.y"
{ yyval.ttype = build_tree_list (build_tree_list (current_declspecs,
							 yyvsp[-1].ttype),
					chainon (yyvsp[0].ttype, all_prefix_attributes));
		  POP_DECLSPEC_STACK; ;
    break;}
case 555:
#line 2530 "objc-parse.y"
{ yyval.ttype = build_tree_list (build_tree_list (current_declspecs,
							 yyvsp[-1].ttype),
					chainon (yyvsp[0].ttype, all_prefix_attributes));
		  POP_DECLSPEC_STACK; ;
    break;}
case 556:
#line 2535 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype;
		  POP_DECLSPEC_STACK; ;
    break;}
case 557:
#line 2538 "objc-parse.y"
{ yyval.ttype = build_tree_list (build_tree_list (current_declspecs,
							 yyvsp[-1].ttype),
					chainon (yyvsp[0].ttype, all_prefix_attributes));
		  POP_DECLSPEC_STACK; ;
    break;}
case 558:
#line 2544 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype;
		  POP_DECLSPEC_STACK; ;
    break;}
case 559:
#line 2552 "objc-parse.y"
{ yyval.ttype = build_tree_list (build_tree_list (current_declspecs,
							 yyvsp[-1].ttype),
					chainon (yyvsp[0].ttype, all_prefix_attributes));
		  POP_DECLSPEC_STACK; ;
    break;}
case 560:
#line 2557 "objc-parse.y"
{ yyval.ttype = build_tree_list (build_tree_list (current_declspecs,
							 yyvsp[-1].ttype),
					chainon (yyvsp[0].ttype, all_prefix_attributes));
		  POP_DECLSPEC_STACK; ;
    break;}
case 561:
#line 2562 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype;
		  POP_DECLSPEC_STACK; ;
    break;}
case 562:
#line 2565 "objc-parse.y"
{ yyval.ttype = build_tree_list (build_tree_list (current_declspecs,
							 yyvsp[-1].ttype),
					chainon (yyvsp[0].ttype, all_prefix_attributes));
		  POP_DECLSPEC_STACK; ;
    break;}
case 563:
#line 2571 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype;
		  POP_DECLSPEC_STACK; ;
    break;}
case 564:
#line 2577 "objc-parse.y"
{ prefix_attributes = chainon (prefix_attributes, yyvsp[-3].ttype);
		  all_prefix_attributes = prefix_attributes; ;
    break;}
case 565:
#line 2586 "objc-parse.y"
{ pushlevel (0);
		  clear_parm_order ();
		  declare_parm_level (1); ;
    break;}
case 566:
#line 2590 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype;
		  parmlist_tags_warning ();
		  poplevel (0, 0, 0); ;
    break;}
case 568:
#line 2598 "objc-parse.y"
{ tree t;
		  for (t = yyvsp[-1].ttype; t; t = TREE_CHAIN (t))
		    if (TREE_VALUE (t) == NULL_TREE)
		      error ("`...' in old-style identifier list");
		  yyval.ttype = tree_cons (NULL_TREE, NULL_TREE, yyvsp[-1].ttype);

		  /* Make sure we have a parmlist after attributes.  */
		  if (yyvsp[-3].ttype != 0
		      && (TREE_CODE (yyval.ttype) != TREE_LIST
			  || TREE_PURPOSE (yyval.ttype) == 0
			  || TREE_CODE (TREE_PURPOSE (yyval.ttype)) != PARM_DECL))
		    YYERROR1;
		;
    break;}
case 569:
#line 2616 "objc-parse.y"
{ yyval.ttype = build_tree_list (NULL_TREE, yyvsp[0].ttype); ;
    break;}
case 570:
#line 2618 "objc-parse.y"
{ yyval.ttype = chainon (yyvsp[-2].ttype, build_tree_list (NULL_TREE, yyvsp[0].ttype)); ;
    break;}
case 571:
#line 2624 "objc-parse.y"
{ yyval.ttype = build_tree_list (NULL_TREE, yyvsp[0].ttype); ;
    break;}
case 572:
#line 2626 "objc-parse.y"
{ yyval.ttype = chainon (yyvsp[-2].ttype, build_tree_list (NULL_TREE, yyvsp[0].ttype)); ;
    break;}
case 573:
#line 2631 "objc-parse.y"
{ yyval.ttype = SAVE_EXT_FLAGS();
		  pedantic = 0;
		  warn_pointer_arith = 0;
		  warn_traditional = 0;
		  flag_iso = 0; ;
    break;}
case 579:
#line 2647 "objc-parse.y"
{
		  if (objc_implementation_context)
                    {
		      finish_class (objc_implementation_context);
		      objc_ivar_chain = NULL_TREE;
		      objc_implementation_context = NULL_TREE;
		    }
		  else
		    warning ("`@end' must appear in an implementation context");
		;
    break;}
case 580:
#line 2662 "objc-parse.y"
{ yyval.ttype = build_tree_list (NULL_TREE, yyvsp[0].ttype); ;
    break;}
case 581:
#line 2664 "objc-parse.y"
{ yyval.ttype = chainon (yyvsp[-2].ttype, build_tree_list (NULL_TREE, yyvsp[0].ttype)); ;
    break;}
case 582:
#line 2669 "objc-parse.y"
{
		  objc_declare_class (yyvsp[-1].ttype);
		;
    break;}
case 583:
#line 2676 "objc-parse.y"
{
		  objc_declare_alias (yyvsp[-2].ttype, yyvsp[-1].ttype);
		;
    break;}
case 584:
#line 2683 "objc-parse.y"
{
		  objc_interface_context = objc_ivar_context
		    = start_class (CLASS_INTERFACE_TYPE, yyvsp[-2].ttype, NULL_TREE, yyvsp[-1].ttype);
                  objc_public_flag = 0;
		;
    break;}
case 585:
#line 2689 "objc-parse.y"
{
                  continue_class (objc_interface_context);
		;
    break;}
case 586:
#line 2694 "objc-parse.y"
{
		  finish_class (objc_interface_context);
		  objc_interface_context = NULL_TREE;
		;
    break;}
case 587:
#line 2700 "objc-parse.y"
{
		  objc_interface_context
		    = start_class (CLASS_INTERFACE_TYPE, yyvsp[-1].ttype, NULL_TREE, yyvsp[0].ttype);
                  continue_class (objc_interface_context);
		;
    break;}
case 588:
#line 2707 "objc-parse.y"
{
		  finish_class (objc_interface_context);
		  objc_interface_context = NULL_TREE;
		;
    break;}
case 589:
#line 2713 "objc-parse.y"
{
		  objc_interface_context = objc_ivar_context
		    = start_class (CLASS_INTERFACE_TYPE, yyvsp[-4].ttype, yyvsp[-2].ttype, yyvsp[-1].ttype);
                  objc_public_flag = 0;
		;
    break;}
case 590:
#line 2719 "objc-parse.y"
{
                  continue_class (objc_interface_context);
		;
    break;}
case 591:
#line 2724 "objc-parse.y"
{
		  finish_class (objc_interface_context);
		  objc_interface_context = NULL_TREE;
		;
    break;}
case 592:
#line 2730 "objc-parse.y"
{
		  objc_interface_context
		    = start_class (CLASS_INTERFACE_TYPE, yyvsp[-3].ttype, yyvsp[-1].ttype, yyvsp[0].ttype);
                  continue_class (objc_interface_context);
		;
    break;}
case 593:
#line 2737 "objc-parse.y"
{
		  finish_class (objc_interface_context);
		  objc_interface_context = NULL_TREE;
		;
    break;}
case 594:
#line 2743 "objc-parse.y"
{
		  objc_implementation_context = objc_ivar_context
		    = start_class (CLASS_IMPLEMENTATION_TYPE, yyvsp[-1].ttype, NULL_TREE, NULL_TREE);
                  objc_public_flag = 0;
		;
    break;}
case 595:
#line 2749 "objc-parse.y"
{
                  objc_ivar_chain
		    = continue_class (objc_implementation_context);
		;
    break;}
case 596:
#line 2755 "objc-parse.y"
{
		  objc_implementation_context
		    = start_class (CLASS_IMPLEMENTATION_TYPE, yyvsp[0].ttype, NULL_TREE, NULL_TREE);
                  objc_ivar_chain
		    = continue_class (objc_implementation_context);
		;
    break;}
case 597:
#line 2763 "objc-parse.y"
{
		  objc_implementation_context = objc_ivar_context
		    = start_class (CLASS_IMPLEMENTATION_TYPE, yyvsp[-3].ttype, yyvsp[-1].ttype, NULL_TREE);
                  objc_public_flag = 0;
		;
    break;}
case 598:
#line 2769 "objc-parse.y"
{
                  objc_ivar_chain
		    = continue_class (objc_implementation_context);
		;
    break;}
case 599:
#line 2775 "objc-parse.y"
{
		  objc_implementation_context
		    = start_class (CLASS_IMPLEMENTATION_TYPE, yyvsp[-2].ttype, yyvsp[0].ttype, NULL_TREE);
                  objc_ivar_chain
		    = continue_class (objc_implementation_context);
		;
    break;}
case 600:
#line 2783 "objc-parse.y"
{
		  objc_interface_context
		    = start_class (CATEGORY_INTERFACE_TYPE, yyvsp[-4].ttype, yyvsp[-2].ttype, yyvsp[0].ttype);
                  continue_class (objc_interface_context);
		;
    break;}
case 601:
#line 2790 "objc-parse.y"
{
		  finish_class (objc_interface_context);
		  objc_interface_context = NULL_TREE;
		;
    break;}
case 602:
#line 2796 "objc-parse.y"
{
		  objc_implementation_context
		    = start_class (CATEGORY_IMPLEMENTATION_TYPE, yyvsp[-3].ttype, yyvsp[-1].ttype, NULL_TREE);
                  objc_ivar_chain
		    = continue_class (objc_implementation_context);
		;
    break;}
case 603:
#line 2806 "objc-parse.y"
{
		  objc_pq_context = 1;
		  objc_interface_context
		    = start_protocol(PROTOCOL_INTERFACE_TYPE, yyvsp[-1].ttype, yyvsp[0].ttype);
		;
    break;}
case 604:
#line 2812 "objc-parse.y"
{
		  objc_pq_context = 0;
		  finish_protocol(objc_interface_context);
		  objc_interface_context = NULL_TREE;
		;
    break;}
case 605:
#line 2821 "objc-parse.y"
{
		  objc_declare_protocols (yyvsp[-1].ttype);
		;
    break;}
case 606:
#line 2828 "objc-parse.y"
{
		  yyval.ttype = NULL_TREE;
		;
    break;}
case 608:
#line 2836 "objc-parse.y"
{
		  if (yyvsp[-2].code == LT_EXPR && yyvsp[0].code == GT_EXPR)
		    yyval.ttype = yyvsp[-1].ttype;
		  else
		    YYERROR1;
		;
    break;}
case 611:
#line 2850 "objc-parse.y"
{ objc_public_flag = 2; ;
    break;}
case 612:
#line 2851 "objc-parse.y"
{ objc_public_flag = 0; ;
    break;}
case 613:
#line 2852 "objc-parse.y"
{ objc_public_flag = 1; ;
    break;}
case 614:
#line 2857 "objc-parse.y"
{
                  yyval.ttype = NULL_TREE;
                ;
    break;}
case 616:
#line 2862 "objc-parse.y"
{
                  if (pedantic)
		    pedwarn ("extra semicolon in struct or union specified");
                ;
    break;}
case 617:
#line 2880 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype;
		  POP_DECLSPEC_STACK; ;
    break;}
case 618:
#line 2883 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype;
		  POP_DECLSPEC_STACK; ;
    break;}
case 619:
#line 2886 "objc-parse.y"
{ yyval.ttype = NULL_TREE; ;
    break;}
case 620:
#line 2891 "objc-parse.y"
{ yyval.ttype = NULL_TREE; ;
    break;}
case 623:
#line 2898 "objc-parse.y"
{
		  yyval.ttype = add_instance_variable (objc_ivar_context,
					      objc_public_flag,
					      yyvsp[0].ttype, current_declspecs,
					      NULL_TREE);
                ;
    break;}
case 624:
#line 2905 "objc-parse.y"
{
		  yyval.ttype = add_instance_variable (objc_ivar_context,
					      objc_public_flag,
					      yyvsp[-2].ttype, current_declspecs, yyvsp[0].ttype);
                ;
    break;}
case 625:
#line 2911 "objc-parse.y"
{
		  yyval.ttype = add_instance_variable (objc_ivar_context,
					      objc_public_flag,
					      NULL_TREE,
					      current_declspecs, yyvsp[0].ttype);
                ;
    break;}
case 626:
#line 2921 "objc-parse.y"
{ objc_inherit_code = CLASS_METHOD_DECL; ;
    break;}
case 627:
#line 2923 "objc-parse.y"
{ objc_inherit_code = INSTANCE_METHOD_DECL; ;
    break;}
case 628:
#line 2928 "objc-parse.y"
{
		  objc_pq_context = 1;
		  if (!objc_implementation_context)
		    fatal_error ("method definition not in class context");
		;
    break;}
case 629:
#line 2934 "objc-parse.y"
{
		  objc_pq_context = 0;
		  if (objc_inherit_code == CLASS_METHOD_DECL)
		    add_class_method (objc_implementation_context, yyvsp[0].ttype);
		  else
		    add_instance_method (objc_implementation_context, yyvsp[0].ttype);
		  start_method_def (yyvsp[0].ttype);
		;
    break;}
case 630:
#line 2943 "objc-parse.y"
{
		  continue_method_def ();
		;
    break;}
case 631:
#line 2947 "objc-parse.y"
{
		  finish_method_def ();
		;
    break;}
case 633:
#line 2958 "objc-parse.y"
{yyval.ttype = NULL_TREE; ;
    break;}
case 638:
#line 2965 "objc-parse.y"
{yyval.ttype = NULL_TREE; ;
    break;}
case 642:
#line 2975 "objc-parse.y"
{
		  /* Remember protocol qualifiers in prototypes.  */
		  objc_pq_context = 1;
		;
    break;}
case 643:
#line 2980 "objc-parse.y"
{
		  /* Forget protocol qualifiers here.  */
		  objc_pq_context = 0;
		  if (objc_inherit_code == CLASS_METHOD_DECL)
		    add_class_method (objc_interface_context, yyvsp[0].ttype);
		  else
		    add_instance_method (objc_interface_context, yyvsp[0].ttype);
		;
    break;}
case 645:
#line 2993 "objc-parse.y"
{
		  yyval.ttype = build_method_decl (objc_inherit_code, yyvsp[-2].ttype, yyvsp[0].ttype, NULL_TREE);
		;
    break;}
case 646:
#line 2998 "objc-parse.y"
{
		  yyval.ttype = build_method_decl (objc_inherit_code, NULL_TREE, yyvsp[0].ttype, NULL_TREE);
		;
    break;}
case 647:
#line 3003 "objc-parse.y"
{
		  yyval.ttype = build_method_decl (objc_inherit_code, yyvsp[-3].ttype, yyvsp[-1].ttype, yyvsp[0].ttype);
		;
    break;}
case 648:
#line 3008 "objc-parse.y"
{
		  yyval.ttype = build_method_decl (objc_inherit_code, NULL_TREE, yyvsp[-1].ttype, yyvsp[0].ttype);
		;
    break;}
case 657:
#line 3038 "objc-parse.y"
{ POP_DECLSPEC_STACK; ;
    break;}
case 658:
#line 3040 "objc-parse.y"
{ shadow_tag (yyvsp[-1].ttype); ;
    break;}
case 659:
#line 3042 "objc-parse.y"
{ pedwarn ("empty declaration"); ;
    break;}
case 660:
#line 3047 "objc-parse.y"
{ push_parm_decl (yyvsp[0].ttype); ;
    break;}
case 661:
#line 3049 "objc-parse.y"
{ push_parm_decl (yyvsp[0].ttype); ;
    break;}
case 662:
#line 3057 "objc-parse.y"
{ yyval.ttype = build_tree_list (build_tree_list (current_declspecs,
							 yyvsp[-1].ttype),
					chainon (yyvsp[0].ttype, all_prefix_attributes)); ;
    break;}
case 663:
#line 3061 "objc-parse.y"
{ yyval.ttype = build_tree_list (build_tree_list (current_declspecs,
							 yyvsp[-1].ttype),
					chainon (yyvsp[0].ttype, all_prefix_attributes)); ;
    break;}
case 664:
#line 3065 "objc-parse.y"
{ yyval.ttype = yyvsp[0].ttype; ;
    break;}
case 665:
#line 3070 "objc-parse.y"
{
	    	  yyval.ttype = NULL_TREE;
		;
    break;}
case 666:
#line 3074 "objc-parse.y"
{
		  /* oh what a kludge! */
		  yyval.ttype = objc_ellipsis_node;
		;
    break;}
case 667:
#line 3079 "objc-parse.y"
{
		  pushlevel (0);
		;
    break;}
case 668:
#line 3083 "objc-parse.y"
{
	  	  /* returns a tree list node generated by get_parm_info */
		  yyval.ttype = yyvsp[0].ttype;
		  poplevel (0, 0, 0);
		;
    break;}
case 671:
#line 3098 "objc-parse.y"
{
		  yyval.ttype = chainon (yyvsp[-1].ttype, yyvsp[0].ttype);
		;
    break;}
case 698:
#line 3120 "objc-parse.y"
{
		  yyval.ttype = build_keyword_decl (yyvsp[-5].ttype, yyvsp[-2].ttype, yyvsp[0].ttype);
		;
    break;}
case 699:
#line 3125 "objc-parse.y"
{
		  yyval.ttype = build_keyword_decl (yyvsp[-2].ttype, NULL_TREE, yyvsp[0].ttype);
		;
    break;}
case 700:
#line 3130 "objc-parse.y"
{
		  yyval.ttype = build_keyword_decl (NULL_TREE, yyvsp[-2].ttype, yyvsp[0].ttype);
		;
    break;}
case 701:
#line 3135 "objc-parse.y"
{
		  yyval.ttype = build_keyword_decl (NULL_TREE, NULL_TREE, yyvsp[0].ttype);
		;
    break;}
case 705:
#line 3148 "objc-parse.y"
{
		  yyval.ttype = chainon (yyvsp[-1].ttype, yyvsp[0].ttype);
		;
    break;}
case 706:
#line 3156 "objc-parse.y"
{
		  if (TREE_CHAIN (yyvsp[0].ttype) == NULL_TREE)
		    /* just return the expr., remove a level of indirection */
		    yyval.ttype = TREE_VALUE (yyvsp[0].ttype);
                  else
		    /* we have a comma expr., we will collapse later */
		    yyval.ttype = yyvsp[0].ttype;
		;
    break;}
case 707:
#line 3168 "objc-parse.y"
{
		  yyval.ttype = build_tree_list (yyvsp[-2].ttype, yyvsp[0].ttype);
		;
    break;}
case 708:
#line 3172 "objc-parse.y"
{
		  yyval.ttype = build_tree_list (NULL_TREE, yyvsp[0].ttype);
		;
    break;}
case 710:
#line 3180 "objc-parse.y"
{
		  yyval.ttype = get_class_reference (yyvsp[0].ttype);
		;
    break;}
case 711:
#line 3187 "objc-parse.y"
{ yyval.ttype = build_tree_list (yyvsp[-2].ttype, yyvsp[-1].ttype); ;
    break;}
case 715:
#line 3198 "objc-parse.y"
{
		  yyval.ttype = chainon (yyvsp[-1].ttype, yyvsp[0].ttype);
		;
    break;}
case 716:
#line 3205 "objc-parse.y"
{
		  yyval.ttype = build_tree_list (yyvsp[-1].ttype, NULL_TREE);
		;
    break;}
case 717:
#line 3209 "objc-parse.y"
{
		  yyval.ttype = build_tree_list (NULL_TREE, NULL_TREE);
		;
    break;}
case 718:
#line 3216 "objc-parse.y"
{
		  yyval.ttype = yyvsp[-1].ttype;
		;
    break;}
case 719:
#line 3223 "objc-parse.y"
{
		  yyval.ttype = yyvsp[-1].ttype;
		;
    break;}
case 720:
#line 3232 "objc-parse.y"
{
		  yyval.ttype = groktypename (yyvsp[-1].ttype);
		;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 498 "/usr/local/share/bison.simple"

  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  int size = 0;
	  char *msg;
	  int x, count;

	  count = 0;
	  /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
	  for (x = (yyn < 0 ? -yyn : 0);
	       x < (sizeof(yytname) / sizeof(char *)); x++)
	    if (yycheck[x + yyn] == x)
	      size += strlen(yytname[x]) + 15, count++;
	  msg = (char *) malloc(size + 15);
	  if (msg != 0)
	    {
	      strcpy(msg, "parse error");

	      if (count < 5)
		{
		  count = 0;
		  for (x = (yyn < 0 ? -yyn : 0);
		       x < (sizeof(yytname) / sizeof(char *)); x++)
		    if (yycheck[x + yyn] == x)
		      {
			strcat(msg, count == 0 ? ", expecting `" : " or `");
			strcat(msg, yytname[x]);
			strcat(msg, "'");
			count++;
		      }
		}
	      yyerror(msg);
	      free(msg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exceeded");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

yyerrhandle:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;
}
#line 3237 "objc-parse.y"


/* yylex() is a thin wrapper around c_lex(), all it does is translate
   cpplib.h's token codes into yacc's token codes.  */

static enum cpp_ttype last_token;

/* The reserved keyword table.  */
struct resword
{
  const char *word;
  ENUM_BITFIELD(rid) rid : 16;
  unsigned int disable   : 16;
};

/* Disable mask.  Keywords are disabled if (reswords[i].disable & mask) is
   _true_.  */
#define D_C89	0x01	/* not in C89 */
#define D_EXT	0x02	/* GCC extension */
#define D_EXT89	0x04	/* GCC extension incorporated in C99 */
#define D_OBJC	0x08	/* Objective C only */

static const struct resword reswords[] =
{
  { "_Bool",		RID_BOOL,	0 },
  { "_Complex",		RID_COMPLEX,	0 },
  { "__FUNCTION__",	RID_FUNCTION_NAME, 0 },
  { "__PRETTY_FUNCTION__", RID_PRETTY_FUNCTION_NAME, 0 },
  { "__alignof",	RID_ALIGNOF,	0 },
  { "__alignof__",	RID_ALIGNOF,	0 },
  { "__asm",		RID_ASM,	0 },
  { "__asm__",		RID_ASM,	0 },
  { "__attribute",	RID_ATTRIBUTE,	0 },
  { "__attribute__",	RID_ATTRIBUTE,	0 },
  { "__bounded",	RID_BOUNDED,	0 },
  { "__bounded__",	RID_BOUNDED,	0 },
  { "__builtin_choose_expr", RID_CHOOSE_EXPR, 0 },
  { "__builtin_types_compatible_p", RID_TYPES_COMPATIBLE_P, 0 },
  { "__builtin_va_arg",	RID_VA_ARG,	0 },
  { "__complex",	RID_COMPLEX,	0 },
  { "__complex__",	RID_COMPLEX,	0 },
  { "__const",		RID_CONST,	0 },
  { "__const__",	RID_CONST,	0 },
  { "__extension__",	RID_EXTENSION,	0 },
  { "__func__",		RID_C99_FUNCTION_NAME, 0 },
  { "__imag",		RID_IMAGPART,	0 },
  { "__imag__",		RID_IMAGPART,	0 },
  { "__inline",		RID_INLINE,	0 },
  { "__inline__",	RID_INLINE,	0 },
  { "__label__",	RID_LABEL,	0 },
  { "__ptrbase",	RID_PTRBASE,	0 },
  { "__ptrbase__",	RID_PTRBASE,	0 },
  { "__ptrextent",	RID_PTREXTENT,	0 },
  { "__ptrextent__",	RID_PTREXTENT,	0 },
  { "__ptrvalue",	RID_PTRVALUE,	0 },
  { "__ptrvalue__",	RID_PTRVALUE,	0 },
  { "__real",		RID_REALPART,	0 },
  { "__real__",		RID_REALPART,	0 },
  { "__restrict",	RID_RESTRICT,	0 },
  { "__restrict__",	RID_RESTRICT,	0 },
  { "__signed",		RID_SIGNED,	0 },
  { "__signed__",	RID_SIGNED,	0 },
  { "__thread",		RID_THREAD,	0 },
  { "__typeof",		RID_TYPEOF,	0 },
  { "__typeof__",	RID_TYPEOF,	0 },
  { "__unbounded",	RID_UNBOUNDED,	0 },
  { "__unbounded__",	RID_UNBOUNDED,	0 },
  { "__volatile",	RID_VOLATILE,	0 },
  { "__volatile__",	RID_VOLATILE,	0 },
  { "asm",		RID_ASM,	D_EXT },
  { "auto",		RID_AUTO,	0 },
  { "break",		RID_BREAK,	0 },
  { "case",		RID_CASE,	0 },
  { "char",		RID_CHAR,	0 },
  { "const",		RID_CONST,	0 },
  { "continue",		RID_CONTINUE,	0 },
  { "default",		RID_DEFAULT,	0 },
  { "do",		RID_DO,		0 },
  { "double",		RID_DOUBLE,	0 },
  { "else",		RID_ELSE,	0 },
  { "enum",		RID_ENUM,	0 },
  { "extern",		RID_EXTERN,	0 },
  { "float",		RID_FLOAT,	0 },
  { "for",		RID_FOR,	0 },
  { "goto",		RID_GOTO,	0 },
  { "if",		RID_IF,		0 },
  { "inline",		RID_INLINE,	D_EXT89 },
  { "int",		RID_INT,	0 },
  { "long",		RID_LONG,	0 },
  { "register",		RID_REGISTER,	0 },
  { "restrict",		RID_RESTRICT,	D_C89 },
  { "return",		RID_RETURN,	0 },
  { "short",		RID_SHORT,	0 },
  { "signed",		RID_SIGNED,	0 },
  { "sizeof",		RID_SIZEOF,	0 },
  { "static",		RID_STATIC,	0 },
  { "struct",		RID_STRUCT,	0 },
  { "switch",		RID_SWITCH,	0 },
  { "typedef",		RID_TYPEDEF,	0 },
  { "typeof",		RID_TYPEOF,	D_EXT },
  { "union",		RID_UNION,	0 },
  { "unsigned",		RID_UNSIGNED,	0 },
  { "void",		RID_VOID,	0 },
  { "volatile",		RID_VOLATILE,	0 },
  { "while",		RID_WHILE,	0 },
  { "id",		RID_ID,			D_OBJC },

  /* These objc keywords are recognized only immediately after
     an '@'.  */
  { "class",		RID_AT_CLASS,		D_OBJC },
  { "compatibility_alias", RID_AT_ALIAS,	D_OBJC },
  { "defs",		RID_AT_DEFS,		D_OBJC },
  { "encode",		RID_AT_ENCODE,		D_OBJC },
  { "end",		RID_AT_END,		D_OBJC },
  { "implementation",	RID_AT_IMPLEMENTATION,	D_OBJC },
  { "interface",	RID_AT_INTERFACE,	D_OBJC },
  { "private",		RID_AT_PRIVATE,		D_OBJC },
  { "protected",	RID_AT_PROTECTED,	D_OBJC },
  { "protocol",		RID_AT_PROTOCOL,	D_OBJC },
  { "public",		RID_AT_PUBLIC,		D_OBJC },
  { "selector",		RID_AT_SELECTOR,	D_OBJC },

  /* These are recognized only in protocol-qualifier context
     (see above) */
  { "bycopy",		RID_BYCOPY,		D_OBJC },
  { "byref",		RID_BYREF,		D_OBJC },
  { "in",		RID_IN,			D_OBJC },
  { "inout",		RID_INOUT,		D_OBJC },
  { "oneway",		RID_ONEWAY,		D_OBJC },
  { "out",		RID_OUT,		D_OBJC },
};
#define N_reswords (sizeof reswords / sizeof (struct resword))

/* Table mapping from RID_* constants to yacc token numbers.
   Unfortunately we have to have entries for all the keywords in all
   three languages.  */
static const short rid_to_yy[RID_MAX] =
{
  /* RID_STATIC */	STATIC,
  /* RID_UNSIGNED */	TYPESPEC,
  /* RID_LONG */	TYPESPEC,
  /* RID_CONST */	TYPE_QUAL,
  /* RID_EXTERN */	SCSPEC,
  /* RID_REGISTER */	SCSPEC,
  /* RID_TYPEDEF */	SCSPEC,
  /* RID_SHORT */	TYPESPEC,
  /* RID_INLINE */	SCSPEC,
  /* RID_VOLATILE */	TYPE_QUAL,
  /* RID_SIGNED */	TYPESPEC,
  /* RID_AUTO */	SCSPEC,
  /* RID_RESTRICT */	TYPE_QUAL,

  /* C extensions */
  /* RID_BOUNDED */	TYPE_QUAL,
  /* RID_UNBOUNDED */	TYPE_QUAL,
  /* RID_COMPLEX */	TYPESPEC,
  /* RID_THREAD */	SCSPEC,

  /* C++ */
  /* RID_FRIEND */	0,
  /* RID_VIRTUAL */	0,
  /* RID_EXPLICIT */	0,
  /* RID_EXPORT */	0,
  /* RID_MUTABLE */	0,

  /* ObjC */
  /* RID_IN */		TYPE_QUAL,
  /* RID_OUT */		TYPE_QUAL,
  /* RID_INOUT */	TYPE_QUAL,
  /* RID_BYCOPY */	TYPE_QUAL,
  /* RID_BYREF */	TYPE_QUAL,
  /* RID_ONEWAY */	TYPE_QUAL,

  /* C */
  /* RID_INT */		TYPESPEC,
  /* RID_CHAR */	TYPESPEC,
  /* RID_FLOAT */	TYPESPEC,
  /* RID_DOUBLE */	TYPESPEC,
  /* RID_VOID */	TYPESPEC,
  /* RID_ENUM */	ENUM,
  /* RID_STRUCT */	STRUCT,
  /* RID_UNION */	UNION,
  /* RID_IF */		IF,
  /* RID_ELSE */	ELSE,
  /* RID_WHILE */	WHILE,
  /* RID_DO */		DO,
  /* RID_FOR */		FOR,
  /* RID_SWITCH */	SWITCH,
  /* RID_CASE */	CASE,
  /* RID_DEFAULT */	DEFAULT,
  /* RID_BREAK */	BREAK,
  /* RID_CONTINUE */	CONTINUE,
  /* RID_RETURN */	RETURN,
  /* RID_GOTO */	GOTO,
  /* RID_SIZEOF */	SIZEOF,

  /* C extensions */
  /* RID_ASM */		ASM_KEYWORD,
  /* RID_TYPEOF */	TYPEOF,
  /* RID_ALIGNOF */	ALIGNOF,
  /* RID_ATTRIBUTE */	ATTRIBUTE,
  /* RID_VA_ARG */	VA_ARG,
  /* RID_EXTENSION */	EXTENSION,
  /* RID_IMAGPART */	IMAGPART,
  /* RID_REALPART */	REALPART,
  /* RID_LABEL */	LABEL,
  /* RID_PTRBASE */	PTR_BASE,
  /* RID_PTREXTENT */	PTR_EXTENT,
  /* RID_PTRVALUE */	PTR_VALUE,

  /* RID_CHOOSE_EXPR */			CHOOSE_EXPR,
  /* RID_TYPES_COMPATIBLE_P */		TYPES_COMPATIBLE_P,

  /* RID_FUNCTION_NAME */		STRING_FUNC_NAME,
  /* RID_PRETTY_FUNCTION_NAME */	STRING_FUNC_NAME,
  /* RID_C99_FUNCTION_NAME */		VAR_FUNC_NAME,

  /* C++ */
  /* RID_BOOL */	TYPESPEC,
  /* RID_WCHAR */	0,
  /* RID_CLASS */	0,
  /* RID_PUBLIC */	0,
  /* RID_PRIVATE */	0,
  /* RID_PROTECTED */	0,
  /* RID_TEMPLATE */	0,
  /* RID_NULL */	0,
  /* RID_CATCH */	0,
  /* RID_DELETE */	0,
  /* RID_FALSE */	0,
  /* RID_NAMESPACE */	0,
  /* RID_NEW */		0,
  /* RID_OPERATOR */	0,
  /* RID_THIS */	0,
  /* RID_THROW */	0,
  /* RID_TRUE */	0,
  /* RID_TRY */		0,
  /* RID_TYPENAME */	0,
  /* RID_TYPEID */	0,
  /* RID_USING */	0,

  /* casts */
  /* RID_CONSTCAST */	0,
  /* RID_DYNCAST */	0,
  /* RID_REINTCAST */	0,
  /* RID_STATCAST */	0,

  /* Objective C */
  /* RID_ID */			OBJECTNAME,
  /* RID_AT_ENCODE */		ENCODE,
  /* RID_AT_END */		END,
  /* RID_AT_CLASS */		CLASS,
  /* RID_AT_ALIAS */		ALIAS,
  /* RID_AT_DEFS */		DEFS,
  /* RID_AT_PRIVATE */		PRIVATE,
  /* RID_AT_PROTECTED */	PROTECTED,
  /* RID_AT_PUBLIC */		PUBLIC,
  /* RID_AT_PROTOCOL */		PROTOCOL,
  /* RID_AT_SELECTOR */		SELECTOR,
  /* RID_AT_INTERFACE */	INTERFACE,
  /* RID_AT_IMPLEMENTATION */	IMPLEMENTATION
};

static void
init_reswords ()
{
  unsigned int i;
  tree id;
  int mask = (flag_isoc99 ? 0 : D_C89)
	      | (flag_no_asm ? (flag_isoc99 ? D_EXT : D_EXT|D_EXT89) : 0);

  if (!flag_objc)
     mask |= D_OBJC;

  /* It is not necessary to register ridpointers as a GC root, because
     all the trees it points to are permanently interned in the
     get_identifier hash anyway.  */
  ridpointers = (tree *) xcalloc ((int) RID_MAX, sizeof (tree));
  for (i = 0; i < N_reswords; i++)
    {
      /* If a keyword is disabled, do not enter it into the table
	 and so create a canonical spelling that isn't a keyword.  */
      if (reswords[i].disable & mask)
	continue;

      id = get_identifier (reswords[i].word);
      C_RID_CODE (id) = reswords[i].rid;
      C_IS_RESERVED_WORD (id) = 1;
      ridpointers [(int) reswords[i].rid] = id;
    }
}

#define NAME(type) cpp_type2name (type)

static void
yyerror (msgid)
     const char *msgid;
{
  const char *string = _(msgid);

  if (last_token == CPP_EOF)
    error ("%s at end of input", string);
  else if (last_token == CPP_CHAR || last_token == CPP_WCHAR)
    {
      unsigned int val = TREE_INT_CST_LOW (yylval.ttype);
      const char *const ell = (last_token == CPP_CHAR) ? "" : "L";
      if (val <= UCHAR_MAX && ISGRAPH (val))
	error ("%s before %s'%c'", string, ell, val);
      else
	error ("%s before %s'\\x%x'", string, ell, val);
    }
  else if (last_token == CPP_STRING
	   || last_token == CPP_WSTRING)
    error ("%s before string constant", string);
  else if (last_token == CPP_NUMBER)
    error ("%s before numeric constant", string);
  else if (last_token == CPP_NAME)
    error ("%s before \"%s\"", string, IDENTIFIER_POINTER (yylval.ttype));
  else
    error ("%s before '%s' token", string, NAME(last_token));
}

static int
yylexname ()
{
  tree decl;

  int objc_force_identifier = objc_need_raw_identifier;
  OBJC_NEED_RAW_IDENTIFIER (0);

  if (C_IS_RESERVED_WORD (yylval.ttype))
    {
      enum rid rid_code = C_RID_CODE (yylval.ttype);

      /* Turn non-typedefed refs to "id" into plain identifiers; this
	 allows constructs like "void foo(id id);" to work.  */
      if (rid_code == RID_ID)
      {
	decl = lookup_name (yylval.ttype);
	if (decl == NULL_TREE || TREE_CODE (decl) != TYPE_DECL)
	  return IDENTIFIER;
      }

      if (!OBJC_IS_AT_KEYWORD (rid_code)
	  && (!OBJC_IS_PQ_KEYWORD (rid_code) || objc_pq_context))
      {
	int yycode = rid_to_yy[(int) rid_code];
	if (yycode == STRING_FUNC_NAME)
	  {
	    /* __FUNCTION__ and __PRETTY_FUNCTION__ get converted
	       to string constants.  */
	    const char *name = fname_string (rid_code);

	    yylval.ttype = build_string (strlen (name) + 1, name);
	    C_ARTIFICIAL_STRING_P (yylval.ttype) = 1;
	    last_token = CPP_STRING;  /* so yyerror won't choke */
	    return STRING;
	  }

	/* Return the canonical spelling for this keyword.  */
	yylval.ttype = ridpointers[(int) rid_code];
	return yycode;
      }
    }

  decl = lookup_name (yylval.ttype);
  if (decl)
    {
      if (TREE_CODE (decl) == TYPE_DECL)
	return TYPENAME;
    }
  else
    {
      tree objc_interface_decl = is_class_name (yylval.ttype);
      /* ObjC class names are in the same namespace as variables and
	 typedefs, and hence are shadowed by local declarations.  */
      if (objc_interface_decl
	  && (global_bindings_p ()
	      || (!objc_force_identifier && !decl)))
	{
	  yylval.ttype = objc_interface_decl;
	  return CLASSNAME;
	}
    }

  return IDENTIFIER;
}

/* Concatenate strings before returning them to the parser.  This isn't quite
   as good as having it done in the lexer, but it's better than nothing.  */

static int
yylexstring ()
{
  enum cpp_ttype next_type;
  tree orig = yylval.ttype;

  next_type = c_lex (&yylval.ttype);
  if (next_type == CPP_STRING
      || next_type == CPP_WSTRING
      || (next_type == CPP_NAME && yylexname () == STRING))
    {
      varray_type strings;


      VARRAY_TREE_INIT (strings, 32, "strings");
      VARRAY_PUSH_TREE (strings, orig);

      do
	{
	  VARRAY_PUSH_TREE (strings, yylval.ttype);
	  next_type = c_lex (&yylval.ttype);
	}
      while (next_type == CPP_STRING
	     || next_type == CPP_WSTRING
	     || (next_type == CPP_NAME && yylexname () == STRING));

      yylval.ttype = combine_strings (strings);
    }
  else
    yylval.ttype = orig;

  /* We will have always read one token too many.  */
  _cpp_backup_tokens (parse_in, 1);

  return STRING;
}

static inline int
_yylex ()
{
 get_next:
  last_token = c_lex (&yylval.ttype);
  switch (last_token)
    {
    case CPP_EQ:					return '=';
    case CPP_NOT:					return '!';
    case CPP_GREATER:	yylval.code = GT_EXPR;		return ARITHCOMPARE;
    case CPP_LESS:	yylval.code = LT_EXPR;		return ARITHCOMPARE;
    case CPP_PLUS:	yylval.code = PLUS_EXPR;	return '+';
    case CPP_MINUS:	yylval.code = MINUS_EXPR;	return '-';
    case CPP_MULT:	yylval.code = MULT_EXPR;	return '*';
    case CPP_DIV:	yylval.code = TRUNC_DIV_EXPR;	return '/';
    case CPP_MOD:	yylval.code = TRUNC_MOD_EXPR;	return '%';
    case CPP_AND:	yylval.code = BIT_AND_EXPR;	return '&';
    case CPP_OR:	yylval.code = BIT_IOR_EXPR;	return '|';
    case CPP_XOR:	yylval.code = BIT_XOR_EXPR;	return '^';
    case CPP_RSHIFT:	yylval.code = RSHIFT_EXPR;	return RSHIFT;
    case CPP_LSHIFT:	yylval.code = LSHIFT_EXPR;	return LSHIFT;

    case CPP_COMPL:					return '~';
    case CPP_AND_AND:					return ANDAND;
    case CPP_OR_OR:					return OROR;
    case CPP_QUERY:					return '?';
    case CPP_OPEN_PAREN:				return '(';
    case CPP_EQ_EQ:	yylval.code = EQ_EXPR;		return EQCOMPARE;
    case CPP_NOT_EQ:	yylval.code = NE_EXPR;		return EQCOMPARE;
    case CPP_GREATER_EQ:yylval.code = GE_EXPR;		return ARITHCOMPARE;
    case CPP_LESS_EQ:	yylval.code = LE_EXPR;		return ARITHCOMPARE;

    case CPP_PLUS_EQ:	yylval.code = PLUS_EXPR;	return ASSIGN;
    case CPP_MINUS_EQ:	yylval.code = MINUS_EXPR;	return ASSIGN;
    case CPP_MULT_EQ:	yylval.code = MULT_EXPR;	return ASSIGN;
    case CPP_DIV_EQ:	yylval.code = TRUNC_DIV_EXPR;	return ASSIGN;
    case CPP_MOD_EQ:	yylval.code = TRUNC_MOD_EXPR;	return ASSIGN;
    case CPP_AND_EQ:	yylval.code = BIT_AND_EXPR;	return ASSIGN;
    case CPP_OR_EQ:	yylval.code = BIT_IOR_EXPR;	return ASSIGN;
    case CPP_XOR_EQ:	yylval.code = BIT_XOR_EXPR;	return ASSIGN;
    case CPP_RSHIFT_EQ:	yylval.code = RSHIFT_EXPR;	return ASSIGN;
    case CPP_LSHIFT_EQ:	yylval.code = LSHIFT_EXPR;	return ASSIGN;

    case CPP_OPEN_SQUARE:				return '[';
    case CPP_CLOSE_SQUARE:				return ']';
    case CPP_OPEN_BRACE:				return '{';
    case CPP_CLOSE_BRACE:				return '}';
    case CPP_ELLIPSIS:					return ELLIPSIS;

    case CPP_PLUS_PLUS:					return PLUSPLUS;
    case CPP_MINUS_MINUS:				return MINUSMINUS;
    case CPP_DEREF:					return POINTSAT;
    case CPP_DOT:					return '.';

      /* The following tokens may affect the interpretation of any
	 identifiers following, if doing Objective-C.  */
    case CPP_COLON:		OBJC_NEED_RAW_IDENTIFIER (0);	return ':';
    case CPP_COMMA:		OBJC_NEED_RAW_IDENTIFIER (0);	return ',';
    case CPP_CLOSE_PAREN:	OBJC_NEED_RAW_IDENTIFIER (0);	return ')';
    case CPP_SEMICOLON:		OBJC_NEED_RAW_IDENTIFIER (0);	return ';';

    case CPP_EOF:
      return 0;

    case CPP_NAME:
      {
	int ret = yylexname ();
	if (ret == STRING)
	  return yylexstring ();
	else
	  return ret;
      }

    case CPP_NUMBER:
    case CPP_CHAR:
    case CPP_WCHAR:
      return CONSTANT;

    case CPP_STRING:
    case CPP_WSTRING:
      return yylexstring ();

      /* This token is Objective-C specific.  It gives the next token
	 special significance.  */
    case CPP_ATSIGN:
      {
	tree after_at;
	enum cpp_ttype after_at_type;

	after_at_type = c_lex (&after_at);

	if (after_at_type == CPP_NAME
	    && C_IS_RESERVED_WORD (after_at)
	    && OBJC_IS_AT_KEYWORD (C_RID_CODE (after_at)))
	  {
	    yylval.ttype = after_at;
	    last_token = after_at_type;
	    return rid_to_yy [(int) C_RID_CODE (after_at)];
	  }
	_cpp_backup_tokens (parse_in, 1);
	return '@';
      }

      /* These tokens are C++ specific (and will not be generated
         in C mode, but let's be cautious).  */
    case CPP_SCOPE:
    case CPP_DEREF_STAR:
    case CPP_DOT_STAR:
    case CPP_MIN_EQ:
    case CPP_MAX_EQ:
    case CPP_MIN:
    case CPP_MAX:
      /* These tokens should not survive translation phase 4.  */
    case CPP_HASH:
    case CPP_PASTE:
      error ("syntax error at '%s' token", NAME(last_token));
      goto get_next;

    default:
      abort ();
    }
  /* NOTREACHED */
}

static int
yylex()
{
  int r;
  timevar_push (TV_LEX);
  r = _yylex();
  timevar_pop (TV_LEX);
  return r;
}

/* Function used when yydebug is set, to print a token in more detail.  */

static void
yyprint (file, yychar, yyl)
     FILE *file;
     int yychar;
     YYSTYPE yyl;
{
  tree t = yyl.ttype;

  fprintf (file, " [%s]", NAME(last_token));

  switch (yychar)
    {
    case IDENTIFIER:
    case TYPENAME:
    case OBJECTNAME:
    case TYPESPEC:
    case TYPE_QUAL:
    case SCSPEC:
    case STATIC:
      if (IDENTIFIER_POINTER (t))
	fprintf (file, " `%s'", IDENTIFIER_POINTER (t));
      break;

    case CONSTANT:
      fprintf (file, " %s", GET_MODE_NAME (TYPE_MODE (TREE_TYPE (t))));
      if (TREE_CODE (t) == INTEGER_CST)
	fprintf (file,
#if HOST_BITS_PER_WIDE_INT == 64
#if HOST_BITS_PER_WIDE_INT == HOST_BITS_PER_INT
		 " 0x%x%016x",
#else
#if HOST_BITS_PER_WIDE_INT == HOST_BITS_PER_LONG
		 " 0x%lx%016lx",
#else
		 " 0x%llx%016llx",
#endif
#endif
#else
#if HOST_BITS_PER_WIDE_INT != HOST_BITS_PER_INT
		 " 0x%lx%08lx",
#else
		 " 0x%x%08x",
#endif
#endif
		 TREE_INT_CST_HIGH (t), TREE_INT_CST_LOW (t));
      break;
    }
}

/* This is not the ideal place to put these, but we have to get them out
   of c-lex.c because cp/lex.c has its own versions.  */

/* Free malloced parser stacks if necessary.  */

void
free_parser_stacks ()
{
  if (malloced_yyss)
    {
      free (malloced_yyss);
      free (malloced_yyvs);
    }
}

#include "gt-c-parse.h"
