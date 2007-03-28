/* packet-dfp.c
 * Routines for Dynamic Feedback Protocol (DFP) dissection
 * (http://www.ietf.org/internet-drafts/draft-eck-dfp-01.txt)
 * Copyright 2006 Marco Molteni (mmolteni at cisco.com)
 *
 * $Id$
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <epan/packet.h>
#include <epan/emem.h>
#include <epan/dissectors/packet-tcp.h>
#include <epan/prefs.h>

#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif

#define DFP_PORT 8081

/* Using a macro allows to insert the name of the calling function */
#define dprintf(fmt, ...) \
    fprintf(stderr, "%s: " fmt "\n", __FUNCTION__, __VA_ARGS__)


/* Forward declaration we need below */
void proto_reg_handoff_dfp(void);

/* Subtree pointers */
static gint ett_dfp      = -1;
static gint ett_tlv      = -1;
static gint ett_hostpref = -1;
static gint ett_security = -1;

/* Protocol and registered fields */
static int proto_dfp                     = -1;
static int hf_dfp_version                = -1;
static int hf_dfp_reserved1              = -1;
static int hf_dfp_type                   = -1;
static int hf_dfp_length                 = -1;
static int hf_tlv                        = -1;
static int hf_tlv_type                   = -1;
static int hf_tlv_length                 = -1;
static int hf_tlv_load_portn             = -1;
static int hf_tlv_load_proto             = -1;
static int hf_tlv_load_flags             = -1;
static int hf_tlv_load_nhosts            = -1;
static int hf_tlv_load_reserved          = -1;
static int hf_tlv_load_hostpref          = -1;
static int hf_tlv_load_hostpref_ipaddr   = -1;
static int hf_tlv_load_hostpref_bind_id  = -1;
static int hf_tlv_load_hostpref_weight   = -1;
static int hf_tlv_security               = -1;
static int hf_tlv_security_algo          = -1;
static int hf_tlv_security_data          = -1;
static int hf_tlv_security_md5           = -1;
static int hf_tlv_security_md5_keyid     = -1;
static int hf_tlv_security_md5_data      = -1;
static int hf_tlv_keepalive_time         = -1;
static int hf_tlv_bind_table_ipaddr      = -1;
static int hf_tlv_bind_table_portn       = -1;
static int hf_tlv_bind_table_proto       = -1;
static int hf_tlv_bind_table_nentries    = -1;
static int hf_tlv_bind_table_bind_field  = -1;


#define DFP_MSG_HEADER_LEN   8
#define DFP_MSG_VERSION      0x01

#define DFP_MSG_PREF_INFO    0x0101
#define DFP_MSG_SERVER_STATE 0x0201
#define DFP_MSG_DFP_PARAMS   0x0301
#define DFP_MSG_BIND_REQ     0x0401
#define DFP_MSG_BIND_REPORT  0x0402
#define DFP_MSG_BIND_CHANGE  0x0403

/* Hack (?) to display "unknown". Maybe there is a better way. */
static const value_string msg_version_names[] = {
    { DFP_MSG_VERSION, "1" },
    { 0, NULL }
};

static const value_string msg_type_names[] = {
    { DFP_MSG_PREF_INFO,    "Preference Info"       },
    { DFP_MSG_SERVER_STATE, "Server State"          },
    { DFP_MSG_DFP_PARAMS,   "Parameters"            },
    { DFP_MSG_BIND_REQ,     "BindID Request"        },
    { DFP_MSG_BIND_REPORT,  "BindID Report"         },
    { DFP_MSG_BIND_CHANGE,  "BindID Change Notify"  },
    { 0, NULL }
};

#define DFP_TLV_SECURITY      0x0001
#define DFP_TLV_LOAD          0x0002
#define DFP_TLV_KEEP_ALIVE    0x0101
/* Reserved: 0x0200-0x02ff */
#define DFP_TLV_BIND_ID_TABLE 0x0301

static const value_string msg_tlv_names[] = {
    { DFP_TLV_SECURITY,      "Security TLV"     },
    { DFP_TLV_LOAD,          "Load TLV"         },
    { DFP_TLV_KEEP_ALIVE,    "Keep Alive TLV"   },
    { DFP_TLV_BIND_ID_TABLE, "BindID Table TLV" },
    { 0, NULL }
};

/* Global sample preference ("controls" display of numbers) */
//static gboolean gPREF_HEX = FALSE;


static void
dissect_tlv_security(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
    gint offset, guint16 tlv_length)
{
    guint16 real_length, remain;

    pinfo = NULL;

    offset += 4; /* skip TLV header */

    proto_tree_add_item(tree, hf_tlv_security_algo, tvb, offset, 4, FALSE);
    offset += 4;

    real_length = min(tlv_length, tvb_length(tvb));
    remain = real_length - 8; /* 8 = security TLV size before security data */
    proto_tree_add_item(tree, hf_tlv_security_data, tvb, offset, remain, FALSE);
    offset += remain;

    /* TODO add MD5 security
     */
}


static void
dissect_tlv_load(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
    gint offset, guint16 tlv_length)
{
    guint16 load_nhosts, real_nhosts, real_length, remain;
    proto_tree *pref_tree = NULL, *ti = NULL;

    pinfo = NULL;

    offset += 4; /* skip TLV header */

    proto_tree_add_item(tree, hf_tlv_load_portn, tvb, offset, 2, FALSE);
    offset += 2;

    proto_tree_add_item(tree, hf_tlv_load_proto, tvb, offset, 1, FALSE);
    offset += 1;

    proto_tree_add_item(tree, hf_tlv_load_flags, tvb, offset, 1, FALSE);
    offset += 1;

    load_nhosts = tvb_get_ntohs(tvb, offset);
    proto_tree_add_item(tree, hf_tlv_load_nhosts, tvb, offset, 2, FALSE);
    offset += 2;

    proto_tree_add_item(tree, hf_tlv_load_reserved, tvb, offset, 2, FALSE);
    offset += 2;

    /* Add a subtree per host preference.
     */
    real_length = min(tlv_length, tvb_length(tvb));
    remain = real_length - 12; /* 12 = load TLV size before hostpref */
    real_nhosts = min(load_nhosts, remain / 8); /* 8 = size hostpref */

    while (real_nhosts-- > 0) {
        /* Create display subtree for host preference.
         */
        ti = proto_tree_add_item(tree, hf_tlv_load_hostpref, tvb, offset, 8, FALSE);
        pref_tree = proto_item_add_subtree(ti, ett_hostpref);

        proto_tree_add_item(pref_tree, hf_tlv_load_hostpref_ipaddr, tvb,
            offset, 4, FALSE);
        offset += 4;

        proto_tree_add_item(pref_tree, hf_tlv_load_hostpref_bind_id, tvb,
            offset, 2, FALSE);
        offset += 2;

        proto_tree_add_item(pref_tree, hf_tlv_load_hostpref_weight, tvb,
            offset, 2, FALSE);
        offset += 2;
    }
}


static void
dissect_tlv_keep_alive(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
    gint offset, guint16 tlv_length)
{
    pinfo = NULL;
    tlv_length = 0;

    offset += 4; /* skip TLV header */

    proto_tree_add_item(tree, hf_tlv_keepalive_time, tvb, offset, 4, FALSE);
    offset += 4;
}


static void
dissect_tlv_bind_id_table(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
    gint offset, guint16 tlv_length)
{
    tvb = NULL;
    pinfo = NULL;
    tree = NULL;
    offset = 0;
    tlv_length = 0;
}


static void
dissect_tlv_unknown(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
    gint offset, guint16 tlv_length)
{
    tvb = NULL;
    pinfo = NULL;
    tree = NULL;
    offset = 0;
    tlv_length = 0;
}


/** Dissect a DFP TLV.
 */
static void
dissect_tlv(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
    gint offset, guint16 tlv_type, guint16 tlv_length)
{
    proto_tree *tlv_tree, *root_ti;
    char       *str;

    /* Create display subtree for the TLV.
     */
    root_ti = proto_tree_add_item(tree, hf_tlv, tvb, offset, tlv_length, FALSE);
    tlv_tree = proto_item_add_subtree(root_ti, ett_tlv);

    proto_tree_add_item(tlv_tree, hf_tlv_type, tvb, offset, 2, FALSE);
    proto_tree_add_item(tlv_tree, hf_tlv_length, tvb, offset + 2, 2, FALSE);

    str = val_to_str(tlv_type, msg_tlv_names, "Unknown TLV (Type 0x%04x)");
    proto_item_set_text(root_ti, "%s", str);
    switch (tlv_type) {

    case DFP_TLV_SECURITY:
        dissect_tlv_security(tvb, pinfo, tlv_tree, offset, tlv_length);
    break;

    case DFP_TLV_LOAD:
        dissect_tlv_load(tvb, pinfo, tlv_tree, offset, tlv_length);
    break;

    case DFP_TLV_KEEP_ALIVE:
        dissect_tlv_keep_alive(tvb, pinfo, tlv_tree, offset, tlv_length);
    break;

    case DFP_TLV_BIND_ID_TABLE:
        dissect_tlv_bind_id_table(tvb, pinfo, tlv_tree, offset, tlv_length);
    break;

    default:
        dissect_tlv_unknown(tvb, pinfo, tlv_tree, offset, tlv_length);
    }
}


/** Dissect a single DFP message.
 */
static void
dfp_dissect_message(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    proto_item *ti = NULL;
    proto_item *root_ti = NULL;
    proto_tree *dfp_tree = NULL;
    gint        offset, remain, real_length;

    /* Message fields */
    guint8  dfp_version = 0;
    guint16 dfp_type    = 0;
    guint32 dfp_length  = 0;
    guint16 tlv_type    = 0;
    guint16 tlv_length  = 0;

    if (check_col(pinfo->cinfo, COL_PROTOCOL)) {
        col_set_str(pinfo->cinfo, COL_PROTOCOL, "DFP");
    }
    if (check_col(pinfo->cinfo, COL_INFO)) {
        col_clear(pinfo->cinfo, COL_INFO);
    }

    dfp_version = tvb_get_guint8(tvb, 0); // TODO validate version
    dfp_type    = tvb_get_ntohs(tvb, 2);  // TODO validate type
    dfp_length  = tvb_get_ntohl(tvb, 4);  // TODO validate len

    if (check_col(pinfo->cinfo, COL_INFO)) {
        col_add_fstr(pinfo->cinfo, COL_INFO, "%s",
            val_to_str(dfp_type, msg_type_names, "Unknown Type (0x%04x)"));
    }

    if (tree) {
        /* Create display subtree for the protocol.
         */
        ti = root_ti = proto_tree_add_item(tree, proto_dfp, tvb, 0, dfp_length, FALSE);
        dfp_tree = proto_item_add_subtree(ti, ett_dfp);

        /* DFP header.
         */
        offset = 0;
        proto_tree_add_item(dfp_tree, hf_dfp_version, tvb, offset, 1, FALSE);
        offset = 1;
        proto_tree_add_item(dfp_tree, hf_dfp_reserved1, tvb, offset, 1, FALSE);
        proto_item_append_text(root_ti, ", %s",
            val_to_str(dfp_type, msg_type_names, "Unknown Type (0x%04x)"));
        offset = 2;
        proto_tree_add_item(dfp_tree, hf_dfp_type, tvb, offset, 2, FALSE);
        offset = 4;
        proto_tree_add_item(dfp_tree, hf_dfp_length, tvb, offset, 4, FALSE);

        /* DFP payload (zero or more TLVs).
         * Note that not all TLVs are allowed with all DFP msg types, but
         * at least for the time being we don't perform this kind of check.
         */
        offset = 8;
        real_length = min(dfp_length, tvb_length(tvb));
        remain = real_length - offset;
        while (remain > 0 && tlv_length > 0) {
            tlv_type = tvb_get_ntohs(tvb, offset);
            tlv_length = tvb_get_ntohs(tvb, offset + 2);
            dissect_tlv(tvb, pinfo, dfp_tree, offset, tlv_type, tlv_length);
            offset += tlv_length;
            remain -= tlv_length;
        }
    }
}


/** Determine PDU length of protocol DFP
 */
static guint
dfp_get_msg_len(tvbuff_t *tvb, int offset)
{
    return tvb_get_ntohl(tvb, offset + 4);
}


/** The main DFP dissecting routine.
 */
static void
dissect_dfp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    int reassemble = TRUE; /* could be set according to preference */

    tcp_dissect_pdus(tvb, pinfo, tree, reassemble, DFP_MSG_HEADER_LEN,
        dfp_get_msg_len, dfp_dissect_message);
}


/* Register the protocol with Wireshark
 */
void
proto_register_dfp(void)
{
    //module_t *dfp_module;

    /* List of header fields */
    static hf_register_info hf[] = {
        /* DFP Header
         */
        { &hf_dfp_version,
            { "Version", "dfp.version",
              FT_UINT8, BASE_DEC, VALS(msg_version_names), 0x0,
              "DFP Protocol Version", HFILL }
        },
        { &hf_dfp_reserved1,
            { "Reserved", "dfp.reserved1",
              FT_UINT8, BASE_DEC, NULL, 0x0,
              "", HFILL }
        },
        { &hf_dfp_type,
            { "Type", "dfp.type",
              FT_UINT16, BASE_HEX, VALS(msg_type_names), 0x0,
              "DFP Message Type", HFILL }
        },
        { &hf_dfp_length,
            { "Length", "dfp.length",
              FT_UINT32, BASE_DEC, NULL, 0x0,
              "DFP Total Length", HFILL }
        },
        /* TLV header
         */
        { &hf_tlv,
            { "TLV", "dfp.tlv",
              FT_NONE, BASE_NONE, NULL, 0x0,
              "DFP TLV", HFILL }
        },
        { &hf_tlv_type,
            { "type", "dfp.tlv_type",
              FT_UINT16, BASE_HEX, VALS(msg_tlv_names), 0x0,
              "DFP TLV type", HFILL }
        },
        { &hf_tlv_length,
            { "length", "dfp.tlv_length",
              FT_UINT16, BASE_DEC, NULL, 0x0,
              "DFP TLV length", HFILL }
        },
        /* Load TLV
         */
        { &hf_tlv_load_portn,
            { "port number", "dfp.load_portn",
              FT_UINT16, BASE_DEC, NULL, 0x0,
              "DFP Load TLV port number", HFILL }
        },
        { &hf_tlv_load_proto,
            { "protocol", "dfp.load_protocol",
              FT_UINT8, BASE_DEC, NULL, 0x0,
              "DFP Load TLV protocol", HFILL }
        },
        { &hf_tlv_load_flags,
            { "flags", "dfp.load_flags",
              FT_UINT8, BASE_HEX, NULL, 0x0,
              "DFP Load TLV flags", HFILL }
        },
        { &hf_tlv_load_nhosts,
            { "number of hosts", "dfp.load_nhosts",
              FT_UINT16, BASE_DEC, NULL, 0x0,
              "DFP Load TLV number of hosts", HFILL }
        },
        { &hf_tlv_load_reserved,
            { "reserved", "dfp.load_reserved",
              FT_UINT16, BASE_HEX, NULL, 0x0,
              "DFP Load TLV reserved", HFILL }
        },
        { &hf_tlv_load_hostpref,
            { "host preference", "dfp.load_hostpref",
              FT_NONE, BASE_HEX, NULL, 0x0,
              "DFP Load TLV host preference", HFILL }
        },
        { &hf_tlv_load_hostpref_ipaddr,
            { "IP address", "dfp.load_ipaddr",
              FT_IPv4, BASE_NONE, NULL, 0x0,
              "DFP Load TLV host preference IP address", HFILL }
        },
        { &hf_tlv_load_hostpref_bind_id,
            { "Bind ID", "dfp.load_bind_id",
              FT_UINT16, BASE_DEC, NULL, 0x0,
              "DFP Load TLV host preference Bind ID", HFILL }
        },
        { &hf_tlv_load_hostpref_weight,
            { "weight", "dfp.load_weight",
              FT_UINT16, BASE_DEC, NULL, 0x0,
              "DFP Load TLV host preference weight", HFILL }
        },
        /* Security TLV
         */
        { &hf_tlv_security_algo,
            { "Algorithm", "dfp.security_algorithm",
              FT_UINT32, BASE_HEX, NULL, 0x0,
              "DFP Security TLV algorithm", HFILL }
        },
        { &hf_tlv_security_data,
            { "Data", "dfp.security_data",
              FT_NONE, BASE_NONE, NULL, 0x0,
              "DFP Security TLV data", HFILL }
        },
        /* Security TLV, MD5 Algorithm
         */
        { &hf_tlv_security_md5_keyid,
            { "MD5 key id", "dfp.security_md5_keyid",
              FT_UINT32, BASE_HEX, NULL, 0x0,
              "DFP Security TLV MD5 key id", HFILL }
        },
        { &hf_tlv_security_md5_data,
            { "MD5 data", "dfp.security_md5_data",
              FT_BYTES, BASE_NONE, NULL, 0x0,
              "DFP Security TLV MD5 data", HFILL }
        },
        /* Keep alive TLV
         */
        { &hf_tlv_keepalive_time,
            { "Time (sec)", "dfp.keepalive_time",
              FT_UINT32, BASE_DEC, NULL, 0x0,
              "DFP Keep alive TLV time", HFILL }
        },
        /* BindID Table TLV
         */
        { &hf_tlv_bind_table_ipaddr,
            { "server address", "dfp.bindtable_ipaddr",
              FT_IPv4, BASE_NONE, NULL, 0x0,
              "DFP BindID Table TLV server address", HFILL }
        },
        { &hf_tlv_bind_table_portn,
            { "port", "dfp.bindtable_port",
              FT_UINT16, BASE_DEC, NULL, 0x0,
              "DFP BindID Table TLV port", HFILL }
        },
        { &hf_tlv_bind_table_proto,
            { "protocol", "dfp.bindtable_proto",
              FT_UINT8, BASE_DEC, NULL, 0x0,
              "DFP BindID Table TLV protocol", HFILL }
        },
        { &hf_tlv_bind_table_nentries,
            { "entries", "dfp.bindtable_entries",
              FT_UINT16, BASE_DEC, NULL, 0x0,
              "DFP BindID Table TLV entries", HFILL }
        },
        { &hf_tlv_bind_table_bind_field,
            { "bind field", "dfp.bindtable_bindfield",
              FT_BYTES, BASE_NONE, NULL, 0x0,
              "DFP BindID Table TLV bind field", HFILL }
        },
    };

    /* Protocol subtree array */
    static gint *ett[] = {
        &ett_dfp,
        &ett_tlv,
        &ett_hostpref,
        &ett_security,
    };

    /* Register the protocol name and description */
    proto_dfp = proto_register_protocol("Dynamic Feedback Protocol",
        "DFP", "dfp");

    /* Required function calls to register the header fields and subtrees used */
    proto_register_field_array(proto_dfp, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));
#if 0
    /* Register preferences module (See Section 2.6 for more on preferences) */
    dfp_module = prefs_register_protocol(proto_dfp, proto_reg_handoff_dfp);

    /* Register a sample preference */
    prefs_register_bool_preference(dfp_module, "showHex",
             "Display numbers in Hex",
         "Enable to display numerical values in hexadecimal.",
         &gPREF_HEX );
#endif
}


/* If this dissector uses sub-dissector registration add a registration routine.
   This exact format is required because a script is used to find these routines
   and create the code that calls these routines.
   
   This function is also called by preferences whenever "Apply" is pressed
   (see prefs_register_protocol above) so it should accommodate being called
   more than once.
*/
void
proto_reg_handoff_dfp(void)
{
    static gboolean inited = FALSE;

    if (!inited) {
        dissector_handle_t dfp_handle;

        dfp_handle = create_dissector_handle(dissect_dfp, proto_dfp);
        dissector_add("tcp.port", DFP_PORT, dfp_handle);

        inited = TRUE;
    }

    /* 
    If you perform registration functions which are dependant upon
    prefs the you should de-register everything which was associated
    with the previous settings and re-register using the new prefs settings
    here. In general this means you need to keep track of what value the
    preference had at the time you registered using a local static in this
    function. ie.

    static int currentPort = -1;

    if( -1 != currentPort ) {
    dissector_delete( "tcp.port", currentPort, dfp_handle);
    }

    currentPort = gPortPref;

    dissector_add("tcp.port", currentPort, dfp_handle);

    */
}
