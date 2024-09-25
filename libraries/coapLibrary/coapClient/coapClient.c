#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include "coapClient.h"

#include "socket.h"
#include "wizchip_conf.h"

//#define DEBUG

#define ACK_TIMEOUT 2000  // 2000ms
#define ACK_RANDOM_FACTOR 1.5  // 타임아웃 랜덤 계수
#define MAX_RETRANSMIT 4  // 최대 재전송 횟수
#define MAX_TRANSMIT_SPAN 45000  // 최대 재전송 시간 (ms 단위)

#define DATA_BUF_SIZE 2048

extern coap_packet_t tx_pkt;

static uint8_t destip[4] = {192, 168, 11, 3};  
static uint16_t destport = 5683;

uint8_t * pCOAP_TX = NULL;
uint8_t * pCOAP_RX = NULL;

static uint8_t COAPSock_Num = 0;

unsigned long MilliTimer;

/*
 * @brief MQTT MilliTimer handler
 * @note MUST BE register to your system 1m Tick timer handler.
 */
void MilliTimer_Handler(void) {
	MilliTimer++;
}

/*
 * @brief Timer Initialize
 * @param  timer : pointer to a Timer structure
 *         that contains the configuration information for the Timer.
 */
void TimerInit(Timer* timer) {
	timer->end_time = 0;
}

/*
 * @brief expired Timer
 * @param  timer : pointer to a Timer structure
 *         that contains the configuration information for the Timer.
 */
char TimerIsExpired(Timer* timer) {
	long left = timer->end_time - MilliTimer;
	return (left < 0);
}

/*
 * @brief Countdown millisecond Timer
 * @param  timer : pointer to a Timer structure
 *         that contains the configuration information for the Timer.
 *         timeout : setting timeout millisecond.
 */
void TimerCountdownMS(Timer* timer, unsigned int timeout) {
	timer->end_time = MilliTimer + timeout;
}

/*
 * @brief Countdown second Timer
 * @param  timer : pointer to a Timer structure
 *         that contains the configuration information for the Timer.
 *         timeout : setting timeout millisecond.
 */
void TimerCountdown(Timer* timer, unsigned int timeout) {
	timer->end_time = MilliTimer + (timeout * 1000);
}

/*
 * @brief left millisecond Timer
 * @param  timer : pointer to a Timer structure
 *         that contains the configuration information for the Timer.
 */
int TimerLeftMS(Timer* timer) {
	long left = timer->end_time - MilliTimer;
	return (left < 0) ? 0 : left;
}

#ifdef DEBUG
void coap_dumpHeader(coap_header_t *hdr)
{
    printf("Header:\n");
    printf("  ver  0x%02X\n", hdr->ver);
    printf("  t    0x%02X\n", hdr->t);
    printf("  tkl  0x%02X\n", hdr->tkl);
    printf("  code 0x%02X\n", hdr->code);
    printf("  id   0x%02X%02X\n", hdr->id[0], hdr->id[1]);
}
#endif

#ifdef DEBUG
void coap_dump(const uint8_t *buf, size_t buflen, bool bare)
{
    if (bare)
    {
        while(buflen--)
            printf("%02X%s", *buf++, (buflen > 0) ? " " : "");
    }
    else
    {
        printf("Dump: ");
        while(buflen--)
            printf("%02X%s", *buf++, (buflen > 0) ? " " : "");
        printf("\n");
    }
}
#endif

int coap_parseHeader(coap_header_t *hdr, const uint8_t *buf, size_t buflen)
{
    if (buflen < 4)
        return COAP_ERR_HEADER_TOO_SHORT;
    hdr->ver = (buf[0] & 0xC0) >> 6;
    if (hdr->ver != 1)
        return COAP_ERR_VERSION_NOT_1;
    hdr->t = (buf[0] & 0x30) >> 4;
    hdr->tkl = buf[0] & 0x0F;
    hdr->code = buf[1];
    hdr->id[0] = buf[2];
    hdr->id[1] = buf[3];
    return 0;
}

int coap_parseToken(coap_buffer_t *tokbuf, const coap_header_t *hdr, const uint8_t *buf, size_t buflen)
{
    if (hdr->tkl == 0)
    {
        tokbuf->p = NULL;
        tokbuf->len = 0;
        return 0;
    }
    else
    if (hdr->tkl <= 8)
    {
        if (4U + hdr->tkl > buflen)
            return COAP_ERR_TOKEN_TOO_SHORT;   // tok bigger than packet
        tokbuf->p = buf+4;  // past header
        tokbuf->len = hdr->tkl;
        return 0;
    }
    else
    {
        // invalid size
        return COAP_ERR_TOKEN_TOO_SHORT;
    }
}

// advances p
int coap_parseOption(coap_option_t *option, uint16_t *running_delta, const uint8_t **buf, size_t buflen)
{
    const uint8_t *p = *buf;
    uint8_t headlen = 1;
    uint16_t len, delta;

    if (buflen < headlen) // too small
        return COAP_ERR_OPTION_TOO_SHORT_FOR_HEADER;

    delta = (p[0] & 0xF0) >> 4;
    len = p[0] & 0x0F;

    // These are untested and may be buggy
    if (delta == 13)
    {
        headlen++;
        if (buflen < headlen)
            return COAP_ERR_OPTION_TOO_SHORT_FOR_HEADER;
        delta = p[1] + 13;
        p++;
    }
    else
    if (delta == 14)
    {
        headlen += 2;
        if (buflen < headlen)
            return COAP_ERR_OPTION_TOO_SHORT_FOR_HEADER;
        delta = ((p[1] << 8) | p[2]) + 269;
        p+=2;
    }
    else
    if (delta == 15)
        return COAP_ERR_OPTION_DELTA_INVALID;

    if (len == 13)
    {
        headlen++;
        if (buflen < headlen)
            return COAP_ERR_OPTION_TOO_SHORT_FOR_HEADER;
        len = p[1] + 13;
        p++;
    }
    else
    if (len == 14)
    {
        headlen += 2;
        if (buflen < headlen)
            return COAP_ERR_OPTION_TOO_SHORT_FOR_HEADER;
        len = ((p[1] << 8) | p[2]) + 269;
        p+=2;
    }
    else
    if (len == 15)
        return COAP_ERR_OPTION_LEN_INVALID;

    if ((p + 1 + len) > (*buf + buflen))
        return COAP_ERR_OPTION_TOO_BIG;

    //printf("option num=%d\n", delta + *running_delta);
    option->num = delta + *running_delta;
    option->buf.p = p+1;
    option->buf.len = len;
    //coap_dump(p+1, len, false);

    // advance buf
    *buf = p + 1 + len;
    *running_delta += delta;

    return 0;
}

// http://tools.ietf.org/html/rfc7252#section-3.1
int coap_parseOptionsAndPayload(coap_option_t *options, uint8_t *numOptions, coap_buffer_t *payload, const coap_header_t *hdr, const uint8_t *buf, size_t buflen)
{
    size_t optionIndex = 0;
    uint16_t delta = 0;
    const uint8_t *p = buf + 4 + hdr->tkl;
    const uint8_t *end = buf + buflen;
    int rc;
    if (p > end)
        return COAP_ERR_OPTION_OVERRUNS_PACKET;   // out of bounds

    //coap_dump(p, end - p);

    // 0xFF is payload marker
    while((optionIndex < *numOptions) && (p < end) && (*p != 0xFF))
    {
        if (0 != (rc = coap_parseOption(&options[optionIndex], &delta, &p, end-p)))
            return rc;
        optionIndex++;
    }
    *numOptions = optionIndex;

    if (p+1 < end && *p == 0xFF)  // payload marker
    {
        payload->p = p+1;
        payload->len = end-(p+1);
    }
    else
    {
        payload->p = NULL;
        payload->len = 0;
    }

    return 0;
}

#ifdef DEBUG
void coap_dumpOptions(coap_option_t *opts, size_t numopt)
{
    size_t i;
    printf(" Options:\n");
    for (i=0;i<numopt;i++)
    {
        printf("  0x%02X [ ", opts[i].num);
        coap_dump(opts[i].buf.p, opts[i].buf.len, true);
        printf(" ]\n");
    }
}
#endif

#ifdef DEBUG
void coap_dumpPacket(coap_packet_t *pkt)
{
    coap_dumpHeader(&pkt->hdr);
    coap_dumpOptions(pkt->opts, pkt->numopts);
    printf("Payload: ");
    coap_dump(pkt->payload.p, pkt->payload.len, true);
    printf("\n");
}
#endif

int coap_parse(coap_packet_t *pkt, const uint8_t *buf, size_t buflen)
{
    int rc;

    // coap_dump(buf, buflen, false);

    if (0 != (rc = coap_parseHeader(&pkt->hdr, buf, buflen)))
        return rc;
//    coap_dumpHeader(&hdr);
    if (0 != (rc = coap_parseToken(&pkt->tok, &pkt->hdr, buf, buflen)))
        return rc;
    pkt->numopts = MAXOPT;
    if (0 != (rc = coap_parseOptionsAndPayload(pkt->opts, &(pkt->numopts), &(pkt->payload), &pkt->hdr, buf, buflen)))
        return rc;
//    coap_dumpOptions(opts, numopt);
    return 0;
}

// options are always stored consecutively, so can return a block with same option num
const coap_option_t *coap_findOptions(const coap_packet_t *pkt, uint8_t num, uint8_t *count)
{
    // FIXME, options is always sorted, can find faster than this
    size_t i;
    const coap_option_t *first = NULL;
    *count = 0;
    for (i=0;i<pkt->numopts;i++)
    {
        if (pkt->opts[i].num == num)
        {
            if (NULL == first)
                first = &pkt->opts[i];
            (*count)++;
        }
        else
        {
            if (NULL != first)
                break;
        }
    }
    return first;
}

int coap_buffer_to_string(char *strbuf, size_t strbuflen, const coap_buffer_t *buf)
{
    if (buf->len+1 > strbuflen)
        return COAP_ERR_BUFFER_TOO_SMALL;
    memcpy(strbuf, buf->p, buf->len);
    strbuf[buf->len] = 0;
    return 0;
}

void coap_option_nibble(uint32_t value, uint8_t *nibble)
{
    if (value<13)
    {
        *nibble = (0xFF & value);
    }
    else
    if (value<=0xFF+13)
    {
        *nibble = 13;
    } else if (value<=0xFFFF+269)
    {
        *nibble = 14;
    }
}

int coap_build(uint8_t *buf, size_t *buflen, const coap_packet_t *pkt)
{
    size_t opts_len = 0;
    size_t i;
    uint8_t *p;
    uint16_t running_delta = 0;

    // build header
    if (*buflen < (4U + pkt->hdr.tkl))
        return COAP_ERR_BUFFER_TOO_SMALL;

    buf[0] = (pkt->hdr.ver & 0x03) << 6;
    buf[0] |= (pkt->hdr.t & 0x03) << 4;
    buf[0] |= (pkt->hdr.tkl & 0x0F);
    buf[1] = pkt->hdr.code;
    buf[2] = pkt->hdr.id[0];
    buf[3] = pkt->hdr.id[1];

    // inject token
    p = buf + 4;
    if ((pkt->hdr.tkl > 0) && (pkt->hdr.tkl != pkt->tok.len))
        return COAP_ERR_UNSUPPORTED;
    
    if (pkt->hdr.tkl > 0)
        memcpy(p, pkt->tok.p, pkt->hdr.tkl);

    // // http://tools.ietf.org/html/rfc7252#section-3.1
    // inject options
    p += pkt->hdr.tkl;

    for (i=0;i<pkt->numopts;i++)
    {
        uint32_t optDelta;
        uint8_t len, delta = 0;

        if (((size_t)(p-buf)) > *buflen)
             return COAP_ERR_BUFFER_TOO_SMALL;
        optDelta = pkt->opts[i].num - running_delta;
        coap_option_nibble(optDelta, &delta);
        coap_option_nibble((uint32_t)pkt->opts[i].buf.len, &len);

        *p++ = (0xFF & (delta << 4 | len));
        if (delta == 13)
        {
            *p++ = (optDelta - 13);
        }
        else
        if (delta == 14)
        {
            *p++ = ((optDelta-269) >> 8);
            *p++ = (0xFF & (optDelta-269));
        }
        if (len == 13)
        {
            *p++ = (pkt->opts[i].buf.len - 13);
        }
        else
        if (len == 14)
  	    {
            *p++ = (pkt->opts[i].buf.len >> 8);
            *p++ = (0xFF & (pkt->opts[i].buf.len-269));
        }
        memcpy(p, pkt->opts[i].buf.p, pkt->opts[i].buf.len);
        p += pkt->opts[i].buf.len;
        running_delta = pkt->opts[i].num;
    }

    opts_len = (p - buf) - 4;   // number of bytes used by options

    if (pkt->payload.len > 0)
    {
        if (*buflen < 4 + 1 + pkt->payload.len + opts_len)
        {
            return COAP_ERR_BUFFER_TOO_SMALL;
        }
        buf[4 + opts_len] = 0xFF;  // payload marker
        memcpy(buf+5 + opts_len, pkt->payload.p, pkt->payload.len);
        *buflen = opts_len + 5 + pkt->payload.len;
    }
    else
        *buflen = opts_len + 4;
    return 0;
}

int coap_make_request(coap_rw_buffer_t *scratch, coap_packet_t *pkt, const uint8_t *uri_path, size_t uri_path_len, const uint8_t *payload, size_t payload_len, uint8_t msgid_hi, uint8_t msgid_lo, const coap_buffer_t* tok, coap_method_t method, coap_content_type_t content_type)
{
    uint32_t scratch_idx = 0;

    pkt->hdr.ver = 0x01;  
    pkt->hdr.t = COAP_TYPE_NONCON;  
    pkt->hdr.tkl = 0;  
    pkt->hdr.code = method;  
    pkt->hdr.id[0] = msgid_hi;  
    pkt->hdr.id[1] = msgid_lo; 
    pkt->numopts = 0;  

    // 요청에 토큰을 포함해야 하는 경우
    if (tok) {
        pkt->hdr.tkl = tok->len;
        pkt->tok = *tok;
    }

    if (uri_path && uri_path_len > 0) {
        const char* delim = "/";
        char* token;
        char uri_path_copy[256];
        strncpy(uri_path_copy, (char *)uri_path, uri_path_len);
        uri_path_copy[uri_path_len] = '\0';  

        if (scratch->len < 2)  
            return COAP_ERR_BUFFER_TOO_SMALL;

        token = strtok(uri_path_copy, delim);

        while (token != NULL) {
            pkt->opts[pkt->numopts].num = COAP_OPTION_URI_PATH;
            pkt->opts[pkt->numopts].buf.p = &scratch->p[scratch_idx];
            memcpy(&scratch->p[scratch_idx], (uint8_t *)token, strlen(token));
            pkt->opts[pkt->numopts].buf.len = strlen(token);
            pkt->numopts++;
            scratch_idx += strlen(token);
            token = strtok(NULL, delim);
        }
    }

    // Content-Format 옵션 추가
    if (content_type != COAP_CONTENTTYPE_NONE) {
        if (scratch->len < 2)  
            return COAP_ERR_BUFFER_TOO_SMALL;
        pkt->opts[pkt->numopts].num = COAP_OPTION_CONTENT_FORMAT;
        pkt->opts[pkt->numopts].buf.p = &scratch->p[scratch_idx];  
        scratch->p[scratch_idx + 0] = ((uint16_t)content_type & 0xFF00) >> 8;
        scratch->p[scratch_idx + 1] = ((uint16_t)content_type & 0x00FF);
        pkt->opts[pkt->numopts].buf.len = 2;
        pkt->numopts++;
    }

    // 페이로드 추가
    if (payload && payload_len > 0) {
        pkt->payload.p = payload;
        pkt->payload.len = payload_len;
    } else {
        pkt->payload.p = NULL;
        pkt->payload.len = 0;
    }

    return 0;  
}

int coap_handle_response(const coap_packet_t *pkt)
{
    uint8_t count;
    const coap_option_t *opt;
    
    // 응답 헤더 처리
    if (pkt->hdr.ver != 1) {
        printf("Unsupported CoAP version: %d\n", pkt->hdr.ver);
        return COAP_ERR_VERSION_NOT_1;
    }
  
    // 응답 코드 확인
    if (pkt->hdr.code == COAP_RSPCODE_NOT_FOUND) {
        printf("Resource not found (404)\n");
        return COAP_ERR_NOT_FOUND;
    } else if (pkt->hdr.code == COAP_RSPCODE_METHOD_NOT_ALLOWED) {
        printf("Method not allowed (405)\n");
        return COAP_ERR_METHOD_NOT_ALLOWED;
    } else if (pkt->hdr.code >= 0x80) {
        printf("Error response code: 0x%02X\n", pkt->hdr.code);
        return COAP_ERR_RESPONSE_CODE;
    }

    // URI-Path 옵션 처리
    opt = coap_findOptions(pkt, COAP_OPTION_URI_PATH, &count);
#ifdef DEBUG
    if (opt && count > 0) {
        printf("Received URI Path: ");
        for (int i = 0; i < count; i++) {
            char uri_path[256];
            memset(uri_path, 0, sizeof(uri_path));
            memcpy(uri_path, opt[i].buf.p, opt[i].buf.len);
            printf("/%s", uri_path);
        }
        printf("\n");
    }
#endif

    // Content-Format 옵션 처리
    opt = coap_findOptions(pkt, COAP_OPTION_CONTENT_FORMAT, &count);
#ifdef DEBUG
    if (opt && count == 1) {
        uint16_t content_format = (opt->buf.p[0] << 8) | opt->buf.p[1];
        printf("Content-Format: %u\n", content_format);
    }
#endif

    // 페이로드 처리
    if (pkt->payload.len > 0) {
#ifdef DEBUG
        printf("Received Payload: ");
        for (size_t i = 0; i < pkt->payload.len; i++) {
            printf("%02X ", pkt->payload.p[i]);  // Hexadecimal 출력
        }
        printf("\n");
#endif

        char payload_str[256];
        memset(payload_str, 0, sizeof(payload_str));
        memcpy(payload_str, pkt->payload.p, pkt->payload.len);
        printf("%s\n", payload_str);
    } else {
        printf("No payload received.\n");
    }

    return 0;  
}

static void coapClient_Sockinit(uint8_t sock)
{
	uint8_t i;

    COAPSock_Num = sock;

}

void coapClient_init(uint8_t * tx_buf, uint8_t * rx_buf, uint8_t sock)
{
	pCOAP_TX = tx_buf;
	pCOAP_RX = rx_buf;

	coapClient_Sockinit(sock);
}


void coapClient_run()
{
    int32_t ret;
    uint16_t size = 0;
    coap_packet_t rx_pkt;
    size_t rsplen = DATA_BUF_SIZE * sizeof(uint8_t);
    Timer timer;
    unsigned int timeout = 0;
    int retransmit_count = 0;  
    int ack_ok = 0;

    if ((ret = coap_build(pCOAP_TX, &rsplen, &tx_pkt)) != 0) {
        printf("Failed to build CoAP request, error code: %d\n", ret);
        return;
    }

    switch (getSn_SR(COAPSock_Num)) {
        case SOCK_UDP:

            ret = sendto(COAPSock_Num, pCOAP_TX, rsplen, destip, destport);
            if (ret < 0) {
                printf("Failed to send request to the server, error code: %ld\n", ret);
                return;
            }

            timeout = ACK_TIMEOUT + (unsigned int)(((double)rand() / RAND_MAX) * (ACK_TIMEOUT * (ACK_RANDOM_FACTOR - 1.0)));

            ack_ok = 0;
            TimerInit(&timer);

            while(retransmit_count < MAX_RETRANSMIT)
            {
                TimerCountdownMS(&timer, timeout);

                while(1)
                {
                    size = getSn_RX_RSR(COAPSock_Num);  
                    if (size > 0) {
                        if (size > DATA_BUF_SIZE) 
                            size = DATA_BUF_SIZE;
            
                        ret = recvfrom(COAPSock_Num, pCOAP_RX, size, destip, &destport);
                        if (ret < 0) {
                            printf("Failed to receive response from the server, error code: %ld\n", ret);
                            return;
                        }

                        if ((ret = coap_parse(&rx_pkt, pCOAP_RX, ret)) != 0) {
                            printf("Failed to parse CoAP response, error code: %d\n", ret);
                            return;
                        }
                        coap_handle_response(&rx_pkt);
                        ack_ok = 1;
                        break;
                    }
                    if(TimerIsExpired(&timer))
                        break;
                }
                if(ack_ok)
                    break;

                retransmit_count++;
                timeout *= 2;  
            }  

            if(!ack_ok)
            {
                printf("Failed to receive response from the server: give up after 4 attempts\n");
            }

            break;
        
        case SOCK_CLOSED:
            if (socket(COAPSock_Num, Sn_MR_UDP, destport, 0x00) == COAPSock_Num) {
                printf("Opened UDP socket, port: %d\n", destport);
            }
            break;
        
        default:
            break;
    }
}
