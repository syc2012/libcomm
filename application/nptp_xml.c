#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "nptp_serv.h"
#include "nptp_proxy.h"


/* Configuration file (.xml) */
#define NPTP_CONF_FILE   "nptp_conf.xml"

/* Tags */
#define TAG_NAME_MAPPING "MAPPING"
#define TAG_NAME_PORT    "PORT"
#define TAG_NAME_PIPE    "PIPE"
#define TAG_NAME_DESC    "DESC"

/* Attributes */
#define ATT_NAME_ENABLE  "enable"


static char *removeSpace(char *pIn)
{
    char *pOut = pIn;
    int i;

    for (i=0; i<strlen(pIn); i++)
    {
        if ((0x0A != *pOut) && (0x0D != *pOut) && (0x20 != *pOut))
        {
            break;
        }
        pOut++;
    }

    for (i=0; i<strlen(pOut); i++)
    {
        if ((0x0A == pOut[i]) || (0x0D == pOut[i]))
        {
            pOut[i] = 0x00;
            break;
        }
    }

    return pOut;
}

static void decomposePathName(char *pIn, char *pPath, char *pName)
{
    int target = -1;
    int i;
    int j;

    for (i=0; i<strlen(pIn); i++)
    {
        if ('/' == pIn[i])
        {
            target = i;
        }
        pPath[i] = pIn[i];
    }

    if (target >= 0)
    {
        pPath[target] = 0x00;
        for (i=0, j=(target+1); j<strlen(pIn); j++)
        {
            pName[i++] = pIn[j];
        }
        pName[i] = 0x00;
    }
    else
    {
        getcwd(pPath, 159);
        strncpy(pName, pIn, 79);
    }
}

static char *getAttribute(xmlNode *pNode, char *pName)
{
    xmlAttr *pAttr = NULL;
    int  found = 0;

    for (pAttr=pNode->properties; pAttr; pAttr=pAttr->next)
    {
        if (0 == strcmp((char *)(pAttr->name), pName))
        {
            found = 1;
            break;
        }
    }

    if ( !found )
    {
        PRINT(
            "<%s>'s attribute '%s' is missing\n",
            (char *)(pNode->name),
            pName
        );
        return NULL;
    }

    if (NULL == pAttr->children)
    {
        PRINT(
            "<%s>'s attribute '%s' is empty\n",
            (char *)(pNode->name),
            pName
        );
        return NULL;
    }

    return (char *)(pAttr->children->content);
}

static void parseTagPort(xmlNode *pParent, tMapping *pMapping)
{
    xmlNode *pNode;
    char *pString;

    for (pNode=pParent->children; pNode; pNode=pNode->next)
    {
        if (XML_ELEMENT_NODE == pNode->type)
        {
            if (0 == strcmp((char *)(pNode->name), TAG_NAME_PORT))
            {
                pString = (char *)xmlNodeGetContent( pNode );

                pMapping->tcpPort = atoi( pString );

                xmlFree( pString );
                break;
            }
        }
    }
}

static void parseTagPipe(xmlNode *pParent, tMapping *pMapping)
{
    xmlNode *pNode;
    char *pString;

    for (pNode=pParent->children; pNode; pNode=pNode->next)
    {
        if (XML_ELEMENT_NODE == pNode->type)
        {
            if (0 == strcmp((char *)(pNode->name), TAG_NAME_PIPE))
            {
                pString = (char *)xmlNodeGetContent( pNode );

                decomposePathName(
                    removeSpace( pString ),
                    pMapping->pipePath,
                    pMapping->pipeName
                );

                xmlFree( pString );
                break;
            }
        }
    }
}

static void parseTagDesc(xmlNode *pParent, tMapping *pMapping)
{
    xmlNode *pNode;
    char *pString;

    for (pNode=pParent->children; pNode; pNode=pNode->next)
    {
        if (XML_ELEMENT_NODE == pNode->type)
        {
            if (0 == strcmp((char *)(pNode->name), TAG_NAME_DESC))
            {
                pString = (char *)xmlNodeGetContent( pNode );

                strncpy(pMapping->descript, removeSpace( pString ), 79);

                xmlFree( pString );
                break;
            }
        }
    }
}

static void parseTagMapping(xmlNode *pParent)
{
    xmlNode *pNode;
    char *pString;
    int enable;

    for (pNode=pParent->children; pNode; pNode=pNode->next)
    {
        if (XML_ELEMENT_NODE == pNode->type)
        {
            if (0 == strcmp((char *)(pNode->name), TAG_NAME_MAPPING))
            {
                pString = (char *)xmlNodeGetContent( pNode );
                xmlFree( pString );

                enable = atoi( getAttribute(pNode, ATT_NAME_ENABLE) );
                if (( enable ) && (g_mappingNum < MAX_MAPPING_NUM))
                {
                    tMapping *pMapping = malloc( sizeof( tMapping ) );

                    memset(pMapping, 0, sizeof( tMapping ));
                    parseTagPort(pNode, pMapping);
                    parseTagPipe(pNode, pMapping);
                    parseTagDesc(pNode, pMapping);
                    pMapping->wd = -1;
                    pMapping->index = g_mappingNum;

                    g_pMapping[g_mappingNum++] = pMapping;
                }
            }
        }
    }
}

int xml_init(void)
{
    xmlDoc *pXmlDoc = NULL;

    /*
     * this initialize the library and check potential ABI mismatches
     * between the version it was compiled for and the actual shared
     * library used.
     */
    LIBXML_TEST_VERSION

    pXmlDoc = xmlReadFile(NPTP_CONF_FILE, NULL, 0);
    if (NULL == pXmlDoc)
    {
        PRINT("ERR: cannot open file %s\n", NPTP_CONF_FILE);
    }
    else
    {
        xmlNode *pNodeRoot = NULL;

        /* Get the root element node */
        pNodeRoot = xmlDocGetRootElement( pXmlDoc );
        if (NULL == pNodeRoot)
        {
            PRINT("ERR: could not get the root element node\n");
            return -1;
        }

        /*
        *  <NPTP>
        *    |-- <MAPPING>
        *    |     |-- <PORT>
        *    |     |-- <PIPE>
        *    |     `-- <DESC>
        *    |-- <MAPPING>
        *    `-- ...
        */
        parseTagMapping( pNodeRoot );

        xmlFreeDoc( pXmlDoc );
    }

    /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();

    return 0;
}

void xml_uninit(void)
{
    ;
}



