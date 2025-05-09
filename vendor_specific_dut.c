/* Copyright (c) 2020 Wi-Fi Alliance                                                */

/* Permission to use, copy, modify, and/or distribute this software for any         */
/* purpose with or without fee is hereby granted, provided that the above           */
/* copyright notice and this permission notice appear in all copies.                */

/* THE SOFTWARE IS PROVIDED 'AS IS' AND THE AUTHOR DISCLAIMS ALL                    */
/* WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                    */
/* WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL                     */
/* THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR                       */
/* CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING                        */
/* FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF                       */
/* CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT                       */
/* OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS                          */
/* SOFTWARE. */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#include "vendor_specific.h"
#include "utils.h"
#ifdef _MXL_
#include <arpa/inet.h>
#include <time.h>

void interfaces_init();
char last_static_ip_addr[64]; /* Only recovery case */
char last_static_ip_if[64];   /* Only recovery case */
#endif

#ifdef HOSTAPD_SUPPORT_MBSSID_WAR
extern int use_openwrt_wpad;
#endif

#ifndef _MXL_
#if defined(_OPENWRT_)
int detect_third_radio() {
    FILE *fp;
    char buffer[BUFFER_LEN];
    int third_radio = 0;

    fp = popen("iw dev", "r");
    if (fp) {
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            if (strstr(buffer, "phy#2"))
                third_radio = 1;
        }
        pclose(fp);
    }

    return third_radio;
}
#endif

void interfaces_init() {
#if defined(_OPENWRT_) && !defined(_WTS_OPENWRT_) && !defined(_MXL_)
    char buffer[BUFFER_LEN];
    char mac_addr[S_BUFFER_LEN];
    int third_radio = 0;

    third_radio = detect_third_radio();

    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "iw phy phy1 interface add ath1 type managed >/dev/null 2>/dev/null");
    system(buffer);
    sprintf(buffer, "iw phy phy1 interface add ath11 type managed >/dev/null 2>/dev/null");
    system(buffer);
    sprintf(buffer, "iw phy phy0 interface add ath0 type managed >/dev/null 2>/dev/null");
    system(buffer);
    sprintf(buffer, "iw phy phy0 interface add ath01 type managed >/dev/null 2>/dev/null");
    system(buffer);
    if (third_radio == 1) {
        sprintf(buffer, "iw phy phy2 interface add ath2 type managed >/dev/null 2>/dev/null");
        system(buffer);
        sprintf(buffer, "iw phy phy2 interface add ath21 type managed >/dev/null 2>/dev/null");
        system(buffer);
    }

    memset(mac_addr, 0, sizeof(mac_addr));
    get_mac_address(mac_addr, sizeof(mac_addr), "ath1");
    control_interface("ath1", "down");
    mac_addr[16] = (char)'0';
    set_mac_address("ath1", mac_addr);

    control_interface("ath11", "down");
    mac_addr[16] = (char)'1';
    set_mac_address("ath11", mac_addr);

    memset(mac_addr, 0, sizeof(mac_addr));
    get_mac_address(mac_addr, sizeof(mac_addr), "ath0");
    control_interface("ath0", "down");
    mac_addr[16] = (char)'0';
    set_mac_address("ath0", mac_addr);

    control_interface("ath01", "down");
    mac_addr[16] = (char)'1';
    set_mac_address("ath01", mac_addr);

    if (third_radio == 1) {
        memset(mac_addr, 0, sizeof(mac_addr));
        get_mac_address(mac_addr, sizeof(mac_addr), "ath2");
        control_interface("ath2", "down");
        mac_addr[16] = (char)'8';
        set_mac_address("ath2", mac_addr);

        control_interface("ath21", "down");
        mac_addr[16] = (char)'9';
        set_mac_address("ath21", mac_addr);
    }
    sleep(1);
#endif
}
#endif /*_MXL_ */

/* Be invoked when start controlApp */
void vendor_init() {
    char buffer[BUFFER_LEN];
    char mac_addr[S_BUFFER_LEN];

    memset(buffer, 0, sizeof(buffer));
#ifndef UPDK
    sprintf(buffer, "brctl show"); //Display bridge pre-wifi down
    system(buffer);
	
	//BELOW LINE IS VERY INMPORTANT: UCI MUST BE DOWN DURING QUICKTRACK TESTING
    system("wifi down >/dev/null 2>/dev/null");
    indigo_logger(LOG_LEVEL_INFO, "interfaces_init: wifi down");
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "brctl show"); //Display bridge post-wifi down
#else
    sprintf(buffer, "/etc/init.d/wpad stop >/dev/null 2>/dev/null");
    system(buffer);
#endif /* UPDK */

    interfaces_init();
#if HOSTAPD_SUPPORT_MBSSID
#ifdef HOSTAPD_SUPPORT_MBSSID_WAR
        system("cp /overlay/hostapd /usr/sbin/hostapd");
        use_openwrt_wpad = 0;
#endif
#endif
}

/* Be invoked when terminate controlApp */
void vendor_deinit() {
    char buffer[S_BUFFER_LEN];
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "killall %s 1>/dev/null 2>/dev/null", get_hapd_exec_file());
    system(buffer);
    sprintf(buffer, "killall %s 1>/dev/null 2>/dev/null", get_wpas_exec_file());
    system(buffer);
}

/* Called by reset_device_hander() */
void vendor_device_reset() {
#ifdef _WTS_OPENWRT_
    char buffer[S_BUFFER_LEN];

    /* Reset the country code */
    snprintf(buffer, sizeof(buffer), "uci -q delete wireless.wifi0.country");
    system(buffer);

    snprintf(buffer, sizeof(buffer), "uci -q delete wireless.wifi1.country");
    system(buffer);
#endif
#if HOSTAPD_SUPPORT_MBSSID
    /* interfaces may be destroyed by hostapd after done the MBSSID testing */
    interfaces_init();
#ifdef HOSTAPD_SUPPORT_MBSSID_WAR
    if (use_openwrt_wpad > 0) {
        system("cp /overlay/hostapd /usr/sbin/hostapd");
        use_openwrt_wpad = 0;
    }
#endif
#endif
}

#ifdef _OPENWRT_
void openwrt_apply_radio_config(void) {
    char buffer[S_BUFFER_LEN];

#ifdef _WTS_OPENWRT_
    // Apply radio configurations
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%s -g /var/run/hostapd/global -B -P /var/run/hostapd-global.pid",
        get_hapd_full_exec_path());
    system(buffer);
    sleep(1);
    system("wifi down >/dev/null 2>/dev/null");
    sleep(2);
    system("wifi up >/dev/null 2>/dev/null");
    sleep(3);

    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "killall %s 1>/dev/null 2>/dev/null", get_hapd_exec_file());
    system(buffer);
    sleep(2);
#endif
}
#endif

/* Called by configure_ap_handler() */
void configure_ap_enable_mbssid() {
#ifdef _WTS_OPENWRT_
    /*
     * the following uci commands need to reboot openwrt
     *    so it can not be configured by controlApp
     * 
     * Manually enable MBSSID on OpenWRT when need to test MBSSID
     * 
    system("uci set wireless.qcawifi=qcawifi");
    system("uci set wireless.qcawifi.mbss_ie_enable=1");
    system("uci commit");
    */
#elif defined(_OPENWRT_)
#ifdef HOSTAPD_SUPPORT_MBSSID_WAR
    system("cp /rom/usr/sbin/wpad /usr/sbin/hostapd");
    use_openwrt_wpad = 1;
#endif
#endif
}

void configure_ap_radio_params(char *band, char *country, int channel, int chwidth) {
#ifdef _WTS_OPENWRT
char buffer[S_BUFFER_LEN], wifi_name[16];

    if (!strncmp(band, "a", 1)) {
        snprintf(wifi_name, sizeof(wifi_name), "wifi0");
    } else {
        snprintf(wifi_name, sizeof(wifi_name), "wifi1");
    }

    if (strlen(country) > 0) {
        snprintf(buffer, sizeof(buffer), "uci set wireless.%s.country=\'%s\'", wifi_name, country);
        system(buffer);
    }

    snprintf(buffer, sizeof(buffer), "uci set wireless.%s.channel=\'%d\'", wifi_name, channel);
    system(buffer);

    if (!strncmp(band, "a", 1)) {
        if (channel == 165) { // Force 20M for CH 165
            snprintf(buffer, sizeof(buffer), "uci set wireless.wifi0.htmode=\'HT20\'");
        } else if (chwidth == 2) { // 160M test cases only
            snprintf(buffer, sizeof(buffer), "uci set wireless.wifi0.htmode=\'HT160\'");
        } else if (chwidth == 0) { // 11N only
            snprintf(buffer, sizeof(buffer), "uci set wireless.wifi0.htmode=\'HT40\'");
        } else { // 11AC or 11AX
            snprintf(buffer, sizeof(buffer), "uci set wireless.wifi0.htmode=\'HT80\'");
        }
        system(buffer);
    }

    system("uci commit");
#endif
}

/* void (*callback_fn)(void *), callback of active wlans iterator
 *
 * Called by start_ap_handler() after invoking hostapd
 */
void start_ap_set_wlan_params(void *if_info) {
    char buffer[S_BUFFER_LEN];
    struct interface_info *wlan = (struct interface_info *) if_info;

    memset(buffer, 0, sizeof(buffer));
#ifdef _WTS_OPENWRT_
    /* Workaround: openwrt has IOT issue with intel AX210 AX mode */
    sprintf(buffer, "cfg80211tool %s he_ul_ofdma 0", wlan->ifname);
    system(buffer);
    /* Avoid target assert during channel switch */
    sprintf(buffer, "cfg80211tool %s he_ul_mimo 0", wlan->ifname);
    system(buffer);
    sprintf(buffer, "cfg80211tool %s twt_responder 0", wlan->ifname);
    system(buffer);
#endif
    printf("set_wlan_params: %s\n", buffer);
}

/* Return addr of P2P-device if there is no GO or client interface */
int get_p2p_mac_addr(char *mac_addr, size_t size) {
    FILE *fp;
    char buffer[S_BUFFER_LEN], *ptr, addr[32];
    int error = 1, match = 0;

    fp = popen("iw dev", "r");
    if (fp) {
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            ptr = strstr(buffer, "addr");
            if (ptr != NULL) {
                sscanf(ptr, "%*s %s", addr);
                while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                    ptr = strstr(buffer, "type");
                    if (ptr != NULL) {
                        ptr += 5;
                        if (!strncmp(ptr, "P2P-GO", 6) || !strncmp(ptr, "P2P-client", 10)) {
			                snprintf(mac_addr, size, "%s", addr);
                            error = 0;
                            match = 1;
                        } else if (!strncmp(ptr, "P2P-device", 10)) {
			                snprintf(mac_addr, size, "%s", addr);
                            error = 0;
                        }
                        break;
                    }
                }
                if (match)
                    break;
            }
        }
        pclose(fp);
    }

    return error;
}

/* Get the name of P2P Group(GO or Client) interface */
int get_p2p_group_if(char *if_name, size_t size) {
    FILE *fp;
    char buffer[S_BUFFER_LEN], *ptr, name[32];
    int error = 1;

    fp = popen("iw dev", "r");
    if (fp) {
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            ptr = strstr(buffer, "Interface");
            if (ptr != NULL) {
                sscanf(ptr, "%*s %s", name);
                while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                    ptr = strstr(buffer, "type");
                    if (ptr != NULL) {
                        ptr += 5;
                        if (!strncmp(ptr, "P2P-GO", 6) || !strncmp(ptr, "P2P-client", 10)) {
			                snprintf(if_name, size, "%s", name);
                            error = 0;
                        }
                        break;
                    }
                }
                if (!error)
                    break;
            }
        }
        pclose(fp);
    }

    return error;
}

/* "iw dev" doesn't show the name of P2P device. The naming rule is based on wpa_supplicant */
int get_p2p_dev_if(char *if_name, size_t size) {
    snprintf(if_name, size, "p2p-dev-%s", get_wireless_interface());

    return 0;
}

/* Append IP range config and start dhcpd */
void start_dhcp_server(char *if_name, char *ip_addr)
{
    char buffer[S_BUFFER_LEN];
    char ip_sub[32], *ptr;
    FILE *fp;

    /* Avoid using system dhcp server service
       snprintf(buffer, sizeof(buffer), "sed -i -e 's/INTERFACESv4=\".*\"/INTERFACESv4=\"%s\"/g' /etc/default/isc-dhcp-server", if_name);
       system(buffer);
       snprintf(buffer, sizeof(buffer), "systemctl restart isc-dhcp-server.service");
       system(buffer);
     */
    /* Sample command from isc-dhcp-server: dhcpd -user dhcpd -group dhcpd -f -4 -pf /run/dhcp-server/dhcpd.pid -cf /etc/dhcp/dhcpd.conf p2p-wlp2s0-0 */

    /* Avoid apparmor check because we manually start dhcpd */
    memset(ip_sub, 0, sizeof(ip_sub));
    ptr = strrchr(ip_addr, '.');
    memcpy(ip_sub, ip_addr, ptr - ip_addr);
    system("cp QT_dhcpd.conf /etc/dhcp/QT_dhcpd.conf");
    fp = fopen("/etc/dhcp/QT_dhcpd.conf", "a");
    if (fp) {
        snprintf(buffer, sizeof(buffer), "\nsubnet %s.0 netmask 255.255.255.0 {\n", ip_sub);
        fputs(buffer, fp);
        snprintf(buffer, sizeof(buffer), "    range %s.50 %s.200;\n", ip_sub, ip_sub);
        fputs(buffer, fp);
        fputs("}\n", fp);
        fclose(fp);
    }
    system("touch /var/lib/dhcp/dhcpd.leases_QT");
    snprintf(buffer, sizeof(buffer), "dhcpd -4 -cf /etc/dhcp/QT_dhcpd.conf -lf /var/lib/dhcp/dhcpd.leases_QT %s", if_name);
    system(buffer);
}

void stop_dhcp_server()
{
    /* system("systemctl stop isc-dhcp-server.service"); */
    system("killall dhcpd 1>/dev/null 2>/dev/null");
}

void start_dhcp_client(char *if_name)
{
    char buffer[S_BUFFER_LEN];

    snprintf(buffer, sizeof(buffer), "dhclient -4 %s &", if_name);
    system(buffer);
}

void stop_dhcp_client()
{
    system("killall dhclient 1>/dev/null 2>/dev/null");
}

wps_setting *p_wps_setting = NULL;
wps_setting customized_wps_settings_ap[AP_SETTING_NUM];
wps_setting customized_wps_settings_sta[STA_SETTING_NUM];

void save_wsc_setting(wps_setting *s, char *entry, int len)
{
    char *p = NULL;

    p = strchr(entry, '\n');
    if (p)
        p++;
    else
        p = entry;

    sscanf(p, "%[^:]:%[^:]:%s", s->wkey, s->value, s->attr);
}

wps_setting* __get_wps_setting(int len, char *buffer, enum wps_device_role role)
{
    char *token = strtok(buffer , ",");
    wps_setting *s = NULL;
    int i = 0;

    if (role == WPS_AP) {
        memset(customized_wps_settings_ap, 0, sizeof(customized_wps_settings_ap));
        p_wps_setting = customized_wps_settings_ap;
        while (token != NULL) {
            s = &p_wps_setting[i++];
            save_wsc_setting(s, token, strlen(token));
            token = strtok(NULL, ",");
        }
    } else {
        memset(customized_wps_settings_sta, 0, sizeof(customized_wps_settings_sta));
        p_wps_setting = customized_wps_settings_sta;
        while (token != NULL) {
            s = &p_wps_setting[i++];
            save_wsc_setting(s, token, strlen(token));
            token = strtok(NULL, ",");
        }
    }
    return p_wps_setting;
}

wps_setting* get_vendor_wps_settings(enum wps_device_role role)
{
    /*
     * Please implement the vendor proprietary function to get WPS OOB and required settings.
     * */
#define WSC_SETTINGS_FILE_AP "/tmp/wsc_settings_APUT"
#define WSC_SETTINGS_FILE_STA "/tmp/wsc_settings_STAUT"
    int len = 0, is_valid = 0;
    char pipebuf[S_BUFFER_LEN];
    char *parameter_ap[] = {"cat", WSC_SETTINGS_FILE_AP, NULL, NULL};
    char *parameter_sta[] = {"cat", WSC_SETTINGS_FILE_STA, NULL, NULL};

    memset(pipebuf, 0, sizeof(pipebuf));
    if (role == WPS_AP) {
        if (0 == access(WSC_SETTINGS_FILE_AP, F_OK)) {
            // use customized ap wsc settings
#ifdef _OPENWRT_
            len = pipe_command(pipebuf, sizeof(pipebuf), "/bin/cat", parameter_ap);
#else
            len = pipe_command(pipebuf, sizeof(pipebuf), "/usr/bin/cat", parameter_ap);
#endif
            if (len) {
                indigo_logger(LOG_LEVEL_INFO, "wsc settings APUT:\n %s", pipebuf);
                return __get_wps_setting(len, pipebuf, WPS_AP);
            } else {
                indigo_logger(LOG_LEVEL_INFO, "wsc settings APUT: no data");
            }
        } else {
            indigo_logger(LOG_LEVEL_ERROR, "APUT: WPS Erorr. Failed to get settings.");
            return NULL;
        }
    } else {
        if (0 == access(WSC_SETTINGS_FILE_STA, F_OK)) {
            // use customized sta wsc settings
            len = pipe_command(pipebuf, sizeof(pipebuf), "/usr/bin/cat", parameter_sta);
            if (len) {
                indigo_logger(LOG_LEVEL_INFO, "wsc settings STAUT:\n %s", pipebuf);
                return __get_wps_setting(len, pipebuf, WPS_STA);
            } else {
                indigo_logger(LOG_LEVEL_INFO, "wsc settings STAUT: no data");
            }
        } else {
            indigo_logger(LOG_LEVEL_ERROR, "STAUT: WPS Erorr. Failed to get settings.");
            return NULL;
        }
    }
}

#ifdef _MXL_
/*
    int wifi_strcpy(char *dest, size_t dest_size, const char *src);
    lightweight replacement for strcpy_s()

    function checks input parameters for null pointers, buffer size, overlapped regions.

    Returns:
         0 - success
        -1 - on any error
*/
int wifi_strcpy(char *dest, size_t dest_size, const char *src)
{
    size_t srclen;

    if (dest == NULL || src == NULL || dest_size == 0)
        return -1;

    dest[0] = '\0';
    srclen = strnlen(src, dest_size);
    if (srclen >= dest_size)
        return -1;

    // Check for overlap
    if ((src >= dest && src < dest + dest_size) ||
        (dest >= src && dest < src + srclen))
        return -1;

    memcpy(dest, src, srclen);
    dest[srclen] = '\0';
    return 0;
}

/*
    int wifi_strcat(char *dest, size_t dest_size, const char *src);
    lightweight replacement for strcat_s()

    function checks input parameters for null pointers, buffer size, overlapped regions.

    Returns:
         0 - success
        -1 - on any error
*/
int wifi_strcat(char *dest, size_t dest_size, const char *src)
{
    size_t destlen, srclen;

    if (dest == NULL || src == NULL || dest_size == 0)
        return -1;

    destlen = strnlen(dest, dest_size);
    srclen = strnlen(src, dest_size);

    if (destlen + srclen >= dest_size)
        return -1;

    // Check for overlap
    if ((src >= dest && src < dest + dest_size) ||
        (dest >= src && dest < src + srclen))
        return -1;

    memcpy(dest + destlen, src, srclen);
    dest[destlen + srclen] = '\0';
    return 0;
}

/*
    int wifi_strncpy(char *dest, size_t dest_size, const char *src, size_t count)
    lightweight replacement for strncpy_s()

    function checks input parameters for null pointers, buffer size, overlapped regions.

    Returns:
         0 - success
        -1 - on any error
*/
int wifi_strncpy(char *dest, size_t dest_size, const char *src, size_t count)
{
    size_t srclen;

    if (dest == NULL || src == NULL || dest_size == 0)
        return -1;

    dest[0] = '\0';
    if (count >= dest_size)
        return -1;

    srclen = strnlen(src, dest_size);
    count = (srclen < count) ? srclen : count;

    // Check for overlap
    if ((src >= dest && src < dest + dest_size) ||
        (dest >= src && dest < src + count))
        return -1;

    memcpy(dest, src, count);
    dest[count] = '\0';
    return 0;
}

static inline size_t wifi_strnlen(const char *src, size_t maxlen) {
    return (src == NULL) ? 0 : strnlen(src, maxlen);
}

#define GET_MAC_ADDR_FILE "/tmp/get_mac_address.sh"
int get_mac_address(char *buffer, int size, char *interface) {

    int len = 0;
    char *parameter[] = {"sh", GET_MAC_ADDR_FILE, interface, NULL};

    memset(buffer, 0, size);
    if (0 == access(GET_MAC_ADDR_FILE, F_OK)) {
        len = pipe_command(buffer, size, "/bin/sh", parameter);
        if (len == 17) { /* Format of MAC addr XX:XX:XX:XX:XX:XX, 17 char */
            indigo_logger(LOG_LEVEL_INFO, "got mac:%s for interface:%s", buffer, interface);
            return 0;
        } else {
            indigo_logger(LOG_LEVEL_INFO, "cant' get mac address for interface %s", interface);
            goto done;
        }
    }

    indigo_logger(LOG_LEVEL_INFO, "file:%s is not exist, cant' get mac for interface:%s", GET_MAC_ADDR_FILE, interface);
done:
	return 1;
}

void interfaces_init() {
#ifdef YOCTO
int len;
  /* for YOCTO we need to stop firewall before each test */
    len = system("sysevent set firewall-stop");
    if (len)
        indigo_logger(LOG_LEVEL_ERROR, "Failed to stop firewall");

#endif /* YOCTO */

    memset(last_static_ip_addr, 0, sizeof(last_static_ip_addr));
    memset(last_static_ip_if, 0, sizeof(last_static_ip_addr));
}

/* For third radio we always need to check wlan4 devices and we can't rely on phy due to
 * phy can be for ZWDFS, wlan4 is always 6G nd wlan6 is ZWDFS
 */
int detect_third_radio() {
    FILE *fp;
    char buffer[BUFFER_LEN];
    int third_radio = 0;

    fp = popen("iw dev", "r");
    if (fp) {
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            if (strstr(buffer, "wlan4"))
                third_radio = 1;
        }
        pclose(fp);
    }
    return third_radio;
}


/* For MXL platforms need to have always first additional VAP. This VAP is only for radio operations,
 * and desn't has BSS functionality. The strong security need to be configured for this VAP. */
extern struct interface_info* band_first_wlan[16];
void mxl_generate_vendor_config(char *output, int size, struct packet_wrapper *wrapper, struct interface_info* wlanp) {

    char buffer[S_BUFFER_LEN], config[S_BUFFER_LEN];
    struct tlv_hdr *tlv = NULL;
    int i;

    if ((wlanp->mbssid_enable && !wlanp->transmitter) || (band_first_wlan[wlanp->band])) {
        snprintf(output, size,
                               "bss=%s\n",
                 wlanp->ifname);
    } else {
        /* get bssid for the first dummy/radio VAP */
        snprintf(config, sizeof(config), "%.5s", wlanp->ifname);
        get_mac_address(buffer, sizeof(buffer), config);

        /* Create section for first dummy/radio VAP - mxl platform specific */
        snprintf(output, size,
                               "ctrl_interface=%s\n"
                               "ctrl_interface_group=0\n"
                               "interface=%.5s\n"
                               "ssid=wave_%.5s\n"
                               "bssid=%s\n"
                               "wpa=2\n"
                               "sae_pwe=2\n"
                               "sae_password=test_passphrase\n"
                               "wpa_pairwise=CCMP\n"
                               "wpa_key_mgmt=SAE\n"
                               "auth_algs=1\n"
                               "ieee80211w=2\n"
                               "bss=%s\n", /* start config section for first functional VAP */
                 HAPD_CTRL_PATH_DEFAULT, wlanp->ifname, wlanp->ifname, buffer, wlanp->ifname);
    }
    /* set bssid and control interface for functional VAP */
    get_mac_address(buffer, sizeof(buffer), wlanp->ifname);
    snprintf(output + strlen(output), size - strlen(output),
                               "bssid=%s\n"
                               "ctrl_interface=%s\n",
             buffer, HAPD_CTRL_PATH_DEFAULT);

    /* For tests with MBO we need to enable auto response for, in this case hostapd will reply
     * on wnm frames.
     */
    for (i = 0; i < wrapper->tlv_num; i++) {
        tlv = wrapper->tlv[i];
        if (tlv->id == TLV_MBO) {
            if (wifi_strcat(output, size, "wnm_bss_trans_query_auto_resp=1\n")) return;
        }
        if (tlv->id == TLV_RSN_PAIRWISE) {
            /* WA for hostapd-legacy, wpa_pairwise must be set for 6GHz */
            if (!strncmp(wlanp->ifname, "wlan4", sizeof("wlan4") - 1)) {
                snprintf(buffer, sizeof(buffer), "wpa_pairwise=%s\n", tlv->value);
                if (wifi_strcat(output, size, buffer))
                    return;
            }
        }
    }
}

char * mxl_config_get_first_vap(char *config, int role) {
    char * first_bss;

    if (role == DUT_TYPE_APUT) {
        first_bss = strstr(config, "bssid=");
        if (!first_bss) {
            indigo_logger(LOG_LEVEL_INFO, "can't find in config radio bss\n", __FUNCTION__, __LINE__);
            return config;
        }
        first_bss = strstr(first_bss  + strlen("bssid="), "bssid=");
        if (!first_bss) {
            indigo_logger(LOG_LEVEL_INFO, "can't find in config VAP bss\n", __FUNCTION__, __LINE__);
            return config;
        }
        first_bss += strlen("bssid=");
        return first_bss;
    } else {
        return config;
    }
}

void mxl_wait_interfaces_started(int len) {
    int i = 0;

      if (len == 0) {
          for (i=0; i < 65; i++) {
              /* wait until one of interfaces will be up */
              if ((system("ifconfig wlan0.0 > /dev/null") == 0) || (system("ifconfig wlan2.0 > /dev/null") == 0) || (system("ifconfig wlan4.0 > /dev/null") == 0))
                  break;
              sleep(1);
          }
      }
}

int mxl_recover_interface_ip(char *local_ip) {

    /* Set IP address */
    set_interface_ip(last_static_ip_if, last_static_ip_addr);
    sleep(1);
    if (find_interface_ip(local_ip, sizeof(local_ip), last_static_ip_if) == 0) {
        indigo_logger(LOG_LEVEL_DEBUG, "IP recovery of %s failed", last_static_ip_if);
        return -1;
    }

    indigo_logger(LOG_LEVEL_DEBUG, "IP recovery of %s passed", last_static_ip_if);
    return 0;
}

void mxl_save_interface_ip(char *ip, char *ifname) {
    /* Save IP and iface name for recovery case */
    snprintf(last_static_ip_addr, sizeof(last_static_ip_addr), "%s", ip);
    snprintf(last_static_ip_if,sizeof(last_static_ip_if), "%s", ifname);
}

void mxl_vendor_get_bss_values(int identifier, int hapd_bss_id, char *connected_ssid, char *mac_addr, char *response) {
    char buff[S_BUFFER_LEN];

    if(identifier >= 0) {
        snprintf(buff, S_BUFFER_LEN, "ssid[%d]", hapd_bss_id + 1);
        get_key_value(connected_ssid, response, buff);
        snprintf(buff, S_BUFFER_LEN, "bssid[%d]", hapd_bss_id + 1);
        get_key_value(mac_addr, response, buff);
    } else {
        get_key_value(connected_ssid, response, "ssid[1]");
        get_key_value(mac_addr, response, "bssid[1]");
    }
}

int insert_time_before_word(char *text, int size, const char *target_word) {
    time_t current_time = time(NULL);
    struct tm *local_time = localtime(&current_time);
    char time_str[S_BUFFER_LEN]; /* format YY:dd:HH:MM:SS */
    char *match;

    strftime(time_str, sizeof(time_str), "_%Y-%d-%H-%M-%S", local_time);
    match = strstr(text, target_word);
    if (match) {
        if (((strlen(time_str) + strlen(text) + 1) > size ))
            return -1;
        memmove(match + strlen(time_str), match, strlen(target_word) + 1);
        memcpy(match, time_str, strlen(time_str));
    }
    return 0;
}

extern struct sockaddr_in *tool_addr;

void mxl_upload_file(char *orig_filename, const char *target_word) {
    char cmd[S_BUFFER_LEN];
    char filename[NAME_SIZE];
    int len;

    memset(cmd, 0, sizeof(cmd));
    memset(filename, 0, sizeof(filename));
    snprintf(filename, sizeof(filename), "%s", orig_filename);
    if (0 != access(filename, F_OK)) {
        indigo_logger(LOG_LEVEL_DEBUG, "file is not available:%s", filename);
        return;
    }
    if (insert_time_before_word(filename, sizeof(filename), target_word)) {
        indigo_logger(LOG_LEVEL_ERROR, "can't filename for:%s, file will be not upload", orig_filename);
        return;
    }
    snprintf(cmd, sizeof(cmd), "cp '%s' '%s'", orig_filename, filename);
    len = system(cmd);
    if (len) {
        indigo_logger(LOG_LEVEL_ERROR, "can't copy file:%s to file:%s", orig_filename, filename);
        return;
    }
    indigo_logger(LOG_LEVEL_INFO, "upload file:%s", filename);
    http_file_post(inet_ntoa(tool_addr->sin_addr), TOOL_POST_PORT, HAPD_UPLOAD_API, filename);
    /* delete copy of file */
    if (remove(filename)) {
        indigo_logger(LOG_LEVEL_ERROR, "can't delete file:%s\n", filename);
        return;
    }
}

void mxl_upload_hostapd_log(void) {

    mxl_upload_file(HAPD_LOG_FILE, ".log");
}

void mxl_upload_wlan_hapd_conf(void *if_info) {
    struct interface_info *wlan = (struct interface_info *) if_info;

    if (!wlan) {
         indigo_logger(LOG_LEVEL_DEBUG, "wlan is not available, can't upload hostapd config");
         return;
    }
    if (!wlan->hapd_conf_file) {
        indigo_logger(LOG_LEVEL_DEBUG, "conf file name is not available, can't upload hostapd config");
        return;
    }

    mxl_upload_file(wlan->hapd_conf_file, ".conf");
}

/* For MXL platforms bridge need to be created and and wlan interface added to it
 * The bridge will be automatically deleted on test stop */
void mxl_create_bridge_and_add_iface(char *output, int size) {
    char cfg_item[S_BUFFER_LEN];
    char *br = NULL;

    br = get_wlans_bridge();
    if ((!br) || (br[0] == '\0')) {
        indigo_logger(LOG_LEVEL_ERROR, "bridge name is not set");
        return;
    }

    if (create_bridge(br)) {
        indigo_logger(LOG_LEVEL_ERROR, "can't create bridge:%s", br);
        return;
    }
    set_interface_ip(br, last_static_ip_addr);
    snprintf(cfg_item, sizeof(cfg_item), "upnp_iface=%s\nbridge=%s\n", br, br);
    if (wifi_strcat(output, size, cfg_item)) {
        indigo_logger(LOG_LEVEL_ERROR, "can't configure upnp bridge:%s\n", br);
        return;
    }
}
#endif