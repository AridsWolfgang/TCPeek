// ============================================================================
// utils.c — Formatting helpers and well-known port service names
// ============================================================================

#include "utils.h"

// ---------------------------------------------------------------------------
// fmt_int — thousands-separated integer formatting
// ---------------------------------------------------------------------------
// Writes the textual representation of n into buf, inserting a comma every
// three digits from the right. At most sz bytes are written (including NUL).

void fmt_int(char *buf, int n, int sz) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%d", n);
    int len = (int)strlen(tmp);
    int pos = 0;
    int remaining = sz - 1;

    for (int i = 0; i < len && remaining > 0; i++) {
        if (i > 0 && (len - i) % 3 == 0) {
            buf[pos++] = ',';
            remaining--;
        }
        buf[pos++] = tmp[i];
        remaining--;
    }
    buf[pos] = 0;
}

// ---------------------------------------------------------------------------
// get_service — well-known TCP port to service name mapping
// ---------------------------------------------------------------------------
// Covers the IANA service range up to port 27017 (MongoDB). Ports not in
// the switch return an empty string, allowing the caller to display nothing.

const char *get_service(int port) {
    switch (port) {
        case 7:     return "ECHO";
        case 20:    return "FTP-DATA";
        case 21:    return "FTP";
        case 22:    return "SSH";
        case 23:    return "TELNET";
        case 25:    return "SMTP";
        case 53:    return "DNS";
        case 67:
        case 68:    return "DHCP";
        case 69:    return "TFTP";
        case 80:    return "HTTP";
        case 110:   return "POP3";
        case 111:   return "RPC";
        case 123:   return "NTP";
        case 135:   return "RPC";
        case 137:
        case 138:   return "NETBIOS";
        case 139:   return "SMB";
        case 143:   return "IMAP";
        case 161:   return "SNMP";
        case 162:   return "SNMP-TRAP";
        case 179:   return "BGP";
        case 389:   return "LDAP";
        case 443:   return "HTTPS";
        case 445:   return "SMB";
        case 465:   return "SMTPS";
        case 514:   return "SYSLOG";
        case 587:   return "SMTP";
        case 636:   return "LDAPS";
        case 993:   return "IMAPS";
        case 995:   return "POP3S";
        case 1433:  return "MSSQL";
        case 1521:  return "ORACLE";
        case 1701:  return "L2TP";
        case 1723:  return "PPTP";
        case 3306:  return "MYSQL";
        case 3389:  return "RDP";
        case 5060:  return "SIP";
        case 5222:  return "XMPP";
        case 5432:  return "POSTGRES";
        case 5900:  return "VNC";
        case 5985:  return "WINRM";
        case 5986:  return "WINRMS";
        case 6379:  return "REDIS";
        case 8080:  return "HTTP-ALT";
        case 8443:  return "HTTPS-ALT";
        case 9090:  return "HTTP-ALT";
        case 27017: return "MONGODB";
        default:    return "";
    }
}
