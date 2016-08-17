/*
 * Copyright © 2012-2015 VMware, Inc.  All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the “License”); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an “AS IS” BASIS, without
 * warranties or conditions of any kind, EITHER EXPRESS OR IMPLIED.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include <includes.h>

uint32_t
VmRESTGetHttpMethod(
    PREST_REQUEST                    pRequest,
    char**                           ppResponse
    )
{
    uint32_t                         dwError = REST_ENGINE_SUCCESS;
    uint32_t                         methodLen = 0;

    if ((pRequest == NULL) || (pRequest->requestLine == NULL) || (ppResponse == NULL))
    {
        VMREST_LOG_DEBUG("VmRESTGetHttpMethod(): Invalid params");
        dwError = VMREST_HTTP_INVALID_PARAMS;
    }
    BAIL_ON_VMREST_ERROR(dwError);

    methodLen = strlen(pRequest->requestLine->method);
    if (methodLen == 0)
    {
        VMREST_LOG_DEBUG("VmRESTGetHttpMethod(): Method seems invalid");
        dwError = VMREST_HTTP_VALIDATION_FAILED;
    }
    BAIL_ON_VMREST_ERROR(dwError);
    *ppResponse = pRequest->requestLine->method;
cleanup:
    return dwError;
error:
    *ppResponse = NULL;
    goto cleanup;
}

uint32_t
VmRESTGetHttpURI(
    PREST_REQUEST                    pRequest,
    char**                           ppResponse
    )
{
    uint32_t                         dwError = REST_ENGINE_SUCCESS;
    uint32_t                         uriLen = 0;

    if ((pRequest == NULL) || (pRequest->requestLine == NULL) || (ppResponse == NULL))
    {
        VMREST_LOG_DEBUG("VmRESTGetHttpURI(): Invalid params");
        dwError = VMREST_HTTP_INVALID_PARAMS;
    }
    BAIL_ON_VMREST_ERROR(dwError);

    uriLen = strlen(pRequest->requestLine->uri);
    if (uriLen == 0 || uriLen > MAX_URI_LEN)
    {
        VMREST_LOG_DEBUG("VmRESTGetHttpURI(): URI seems invalid in request object");
        dwError = VMREST_HTTP_VALIDATION_FAILED;
    }
    BAIL_ON_VMREST_ERROR(dwError);
    *ppResponse = pRequest->requestLine->uri;

cleanup:
    return dwError;
error:
    *ppResponse = NULL;
    goto cleanup;
}

uint32_t
VmRESTGetHttpVersion(
    PREST_REQUEST                    pRequest,
    char**                           ppResponse
    )
{
    uint32_t                         dwError = REST_ENGINE_SUCCESS;
    uint32_t                         versionLen = 0;

    if ((pRequest == NULL) || (pRequest->requestLine == NULL) || (ppResponse == NULL))
    {
        VMREST_LOG_DEBUG("VmRESTGetHttpVersion(): Invalid params");
        dwError = VMREST_HTTP_INVALID_PARAMS;
    }
    BAIL_ON_VMREST_ERROR(dwError);

    versionLen = strlen(pRequest->requestLine->version);
    if (versionLen == 0 || versionLen > MAX_VERSION_LEN)
    {
        VMREST_LOG_DEBUG("VmRESTGetHttpVersion(): Version info invalid in request object");
        dwError = VMREST_HTTP_VALIDATION_FAILED;
    }
    BAIL_ON_VMREST_ERROR(dwError);
    *ppResponse = pRequest->requestLine->version;

cleanup:
    return dwError;
error:
    *ppResponse = NULL;
    goto cleanup;
}

uint32_t
VmRESTGetHttpHeader(
    PREST_REQUEST                    pRequest,
    char const*                      header,
    char**                           ppResponse
    )
{
    uint32_t                         dwError = REST_ENGINE_SUCCESS;
    uint32_t                         headerValLen = 0;

    if ((pRequest == NULL) || (header == NULL) || (ppResponse == NULL))
    {
        VMREST_LOG_DEBUG("VmRESTGetHttpHeader(): Invalid params");
        dwError = VMREST_HTTP_INVALID_PARAMS;
    }
    BAIL_ON_VMREST_ERROR(dwError);

    dwError = VmRESTGetHTTPMiscHeader(
                  pRequest->miscHeader,
                  header,
                  ppResponse
                  );
    BAIL_ON_VMREST_ERROR(dwError);
    if (*ppResponse)
    {
         headerValLen = strlen(*ppResponse);
         if (headerValLen < 0 || headerValLen > MAX_HTTP_HEADER_VAL_LEN)
         {
             dwError = VMREST_HTTP_VALIDATION_FAILED;
         }
    }
    else
    {
        VMREST_LOG_DEBUG("WARNING :: Header %s not found in request object", header);
    }
    BAIL_ON_VMREST_ERROR(dwError);

cleanup:
    return dwError;
error:
    *ppResponse = NULL;
    goto cleanup;
}

uint32_t
VmRESTGetHttpPayload(
    PREST_REQUEST                    pRequest,
    char*                            response,
    uint32_t*                        done
    )
{

    uint32_t                         dwError = REST_ENGINE_SUCCESS;
    uint32_t                         dataRemaining = 0;
    char                             localAppBuffer[MAX_DATA_BUFFER_LEN] = {0};
    uint32_t                         chunkLenBytes = 0;
    uint32_t                         chunkLen = 0;
    uint32_t                         bytesRead = 0;
    uint32_t                         readXBytes = 0;
    uint32_t                         actualBytesCopied = 0;
    uint32_t                         newChunk = 0;
    uint32_t                         extraRead = 0;
    char*                            res = NULL;
    char*                            contentLength = NULL;
    char*                            transferEncoding = NULL;

    if (pRequest == NULL || response == NULL || done == NULL)
    {
        VMREST_LOG_DEBUG("Invalid params");
        dwError = VMREST_HTTP_INVALID_PARAMS;
    }
    BAIL_ON_VMREST_ERROR(dwError);
    *done = 0;

    if (sizeof(response) > MAX_DATA_BUFFER_LEN)
    {
        VMREST_LOG_DEBUG("Response buffer size %u not large enough",sizeof(response));
        dwError = VMREST_HTTP_INVALID_PARAMS;
    }
    BAIL_ON_VMREST_ERROR(dwError);
    memset(localAppBuffer,'\0', MAX_DATA_BUFFER_LEN);

    dwError = VmRESTGetHttpHeader(
                  pRequest,
                  "Content-Length",
                  &contentLength
                  );
    BAIL_ON_VMREST_ERROR(dwError);

    dwError = VmRESTGetHttpHeader(
                  pRequest,
                  "Transfer-Encoding",
                  &transferEncoding
                  );
    BAIL_ON_VMREST_ERROR(dwError);

    if (contentLength != NULL && strlen(contentLength) > 0)
    {
       /**** Content-Length based packets ****/

        dataRemaining = pRequest->dataRemaining;
        if ((dataRemaining > 0) && (dataRemaining <= MAX_DATA_BUFFER_LEN))
        {
            readXBytes = dataRemaining;
        }
        else if (dataRemaining > MAX_DATA_BUFFER_LEN)
        {
            readXBytes = MAX_DATA_BUFFER_LEN;
        }
        VMREST_LOG_DEBUG("DEBUG: Get Payload, Requesting %u bytes to read", readXBytes);
        dwError = VmsockPosixGetXBytes(
                      readXBytes,
                      localAppBuffer,
                      pRequest->clientIndex,
                      &bytesRead,
                      1
                      );
        BAIL_ON_VMREST_ERROR(dwError);

        dwError = VmRESTCopyDataWithoutCRLF(
                      bytesRead,
                      localAppBuffer,
                      response,
                      &actualBytesCopied
                      );
        BAIL_ON_VMREST_ERROR(dwError);
        pRequest->dataRemaining = pRequest->dataRemaining - actualBytesCopied;

        if (pRequest->dataRemaining == 0)
        {
            *done = 1;
        }
        if (bytesRead == 0 && readXBytes != 0)
        {
            dwError = VMREST_HTTP_VALIDATION_FAILED;
            VMREST_LOG_DEBUG("ERROR :: No data available over socket to read");
            *done = 1;
        }
    }
    else if((transferEncoding != NULL) && (strcmp(transferEncoding,"chunked")) == 0)
    {
        res = response;
        dataRemaining = pRequest->dataRemaining;
        if (dataRemaining == 0)
        {
            readXBytes = HTTP_CHUNCKED_DATA_LEN;
            newChunk = 1;
        }
        else if (dataRemaining > MAX_DATA_BUFFER_LEN)
        {
            readXBytes = MAX_DATA_BUFFER_LEN;
            newChunk = 0;
        }
        else if (dataRemaining < MAX_DATA_BUFFER_LEN)
        {
            readXBytes = dataRemaining;
            newChunk = 0;
        }

        /**** This is chunked encoded packet ****/
        dwError = VmsockPosixGetXBytes(
                      readXBytes,
                      localAppBuffer,
                      pRequest->clientIndex,
                      &bytesRead,
                      1
                      );
        BAIL_ON_VMREST_ERROR(dwError);


        if (newChunk)
        {
            dwError = VmRESTGetChunkSize(
                          localAppBuffer,
                          &chunkLenBytes,
                          &chunkLen
                          );
            BAIL_ON_VMREST_ERROR(dwError);
            pRequest->dataRemaining = chunkLen;

            if (chunkLen == 0)
            {
                *done = 1;
            }
            if (*done == 0)
            {
                /**** Copy the extra data from last read if it exists ****/
                extraRead = bytesRead - chunkLenBytes;
                if (extraRead > 0)
                {
                    memcpy(res, (localAppBuffer + chunkLenBytes), extraRead);
                    res = res + extraRead;
                    pRequest->dataRemaining = pRequest->dataRemaining - extraRead;
                }

                memset(localAppBuffer,'\0',MAX_DATA_BUFFER_LEN);

                if (pRequest->dataRemaining > (MAX_DATA_BUFFER_LEN - extraRead))
                {
                    readXBytes = MAX_DATA_BUFFER_LEN -extraRead;
                }
                else if (pRequest->dataRemaining <= (MAX_DATA_BUFFER_LEN - extraRead))
                {
                    readXBytes = pRequest->dataRemaining;
                }
                dwError = VmsockPosixGetXBytes(
                              readXBytes,
                              localAppBuffer,
                              pRequest->clientIndex,
                              &bytesRead,
                              1
                              );
                BAIL_ON_VMREST_ERROR(dwError);

                dwError = VmRESTCopyDataWithoutCRLF(
                              bytesRead,
                              localAppBuffer,
                              res,
                              &actualBytesCopied
                              );
                BAIL_ON_VMREST_ERROR(dwError);
                pRequest->dataRemaining = pRequest->dataRemaining - actualBytesCopied;

                /**** Read the /r/n succeeding the chunk ****/
                if (pRequest->dataRemaining == 0)
                {
                    dwError = VmsockPosixGetXBytes(
                                  2,
                                  localAppBuffer,
                                  pRequest->clientIndex,
                                  &bytesRead,
                                  0
                                  );
                BAIL_ON_VMREST_ERROR(dwError);
                }
            }
        }
    }
    else
    {
        VMREST_LOG_DEBUG("WARNING: Data length Specific Header not set");
        *done = 1;
    }
    BAIL_ON_VMREST_ERROR(dwError);

cleanup:
    return dwError;
error:
    response = NULL;
    goto cleanup;
}

uint32_t
VmRESTSetHttpPayload(
    PREST_RESPONSE*                  ppResponse,
    char*                            buffer,
    uint32_t                         dataLen,
    uint32_t*                        done
    )
{
    uint32_t                         dwError = REST_ENGINE_SUCCESS;
    uint32_t                         contentLen = 0;
    PREST_RESPONSE                   pResponse = NULL;
    char*                            contentLength = NULL;
    char*                            transferEncoding = NULL;


    if (ppResponse == NULL || *ppResponse == NULL || buffer == NULL || done == NULL)
    {
        VMREST_LOG_DEBUG("Invalid params");
        dwError = VMREST_HTTP_INVALID_PARAMS;
    }
    BAIL_ON_VMREST_ERROR(dwError);

    pResponse = *ppResponse;
    *done = 0;

    dwError = VmRESTGetHttpResponseHeader(
                  pResponse,
                  "Content-Length",
                  &contentLength
                  );
    BAIL_ON_VMREST_ERROR(dwError);

    dwError = VmRESTGetHttpResponseHeader(
                  pResponse,
                  "Transfer-Encoding",
                  &transferEncoding
                  );
    BAIL_ON_VMREST_ERROR(dwError);

    /**** Either of Content-Length or chunked-Encoding header must be set ****/
    if ((contentLength != NULL) && (strlen(contentLength) > 0))
    {
        contentLen = atoi(contentLength);
        if ((contentLen >= 0) && (contentLen <= MAX_DATA_BUFFER_LEN))
        {
            memcpy(pResponse->messageBody->buffer, buffer, contentLen);
        }
        else
        {
            VMREST_LOG_DEBUG("Invalid content length %u", contentLen);
            dwError = VMREST_HTTP_VALIDATION_FAILED;
        }
        dwError = VmRESTSendHeaderAndPayload(
                      ppResponse
                      );
        VMREST_LOG_DEBUG("Sending Header and Payload done, returned code %u", dwError);
        BAIL_ON_VMREST_ERROR(dwError);
        pResponse->headerSent = 1;
        *done = 1;
    }
    else if ((transferEncoding != NULL) && (strcmp(transferEncoding, "chunked") == 0))
    {
         if (dataLen > MAX_DATA_BUFFER_LEN)
         {
             VMREST_LOG_DEBUG("Chunked data length %u not allowed", dataLen);
             dwError = VMREST_HTTP_VALIDATION_FAILED;
         }
         BAIL_ON_VMREST_ERROR(dwError);

         memcpy(pResponse->messageBody->buffer, buffer, dataLen);
         if (pResponse->headerSent == 0)
         {
             /**** Send Header first ****/
             dwError = VmRESTSendHeader(
                           ppResponse
                           );
             BAIL_ON_VMREST_ERROR(dwError);
             pResponse->headerSent = 1;
         }
         dwError = VmRESTSendChunkedPayload(
                       ppResponse,
                       dataLen
                       );
         BAIL_ON_VMREST_ERROR(dwError);
         if (dataLen == 0)
         {
             *done = 1;
         }
    }
    else
    {
        VMREST_LOG_DEBUG("Content length or TransferEncoding");
        dwError = VMREST_HTTP_VALIDATION_FAILED;
    }
    BAIL_ON_VMREST_ERROR(dwError);

cleanup:
    return dwError;
error:
    VMREST_LOG_DEBUG("SomeThing failed");
    goto cleanup;
}


uint32_t
VmRESTSetHttpHeader(
    PREST_RESPONSE*                  ppResponse,
    char const*                      header,
    char*                            value
    )
{
    uint32_t                         dwError = REST_ENGINE_SUCCESS;
    PREST_RESPONSE                   pResponse = NULL;

    if (ppResponse == NULL || *ppResponse == NULL || header == NULL || value == NULL)
    {
        VMREST_LOG_DEBUG("VmRESTSetHttpHeader(): Invalid params");
        dwError = VMREST_HTTP_INVALID_PARAMS;
    }
    BAIL_ON_VMREST_ERROR(dwError);

    pResponse = *ppResponse;

    dwError = VmRESTSetHTTPMiscHeader(
                  pResponse->miscHeader,
                  header,
                  value
                  );
    BAIL_ON_VMREST_ERROR(dwError);
cleanup:
    return dwError;
error:
    goto cleanup;
}

uint32_t
VmRESTSetHttpStatusCode(
    PREST_RESPONSE*                  ppResponse,
    char*                            statusCode
    )
{
    uint32_t                         dwError = REST_ENGINE_SUCCESS;
    uint32_t                         statusLen = 0;
    PREST_RESPONSE    pResponse = NULL;

    if (ppResponse == NULL || *ppResponse == NULL || statusCode == NULL)
    {
        /* Response object not allocated any memory */
        VMREST_LOG_DEBUG("VmRESTSetHttpStatusCode(): Invalid params");
        dwError = VMREST_HTTP_INVALID_PARAMS;
    }
    BAIL_ON_VMREST_ERROR(dwError);

    pResponse = *ppResponse;
    statusLen = strlen(statusCode);

    if (statusLen >= MAX_STATUS_LEN)
    {
        VMREST_LOG_DEBUG("VmRESTSetHttpStatusCode(): Status length too large");
        dwError = VMREST_HTTP_VALIDATION_FAILED;
    }
    BAIL_ON_VMREST_ERROR(dwError);
    /* TODO :: Check validity of Status Code */

    strcpy(pResponse->statusLine->statusCode, statusCode);

cleanup:
    return dwError;
error:
    goto cleanup;
}

uint32_t
VmRESTSetHttpStatusVersion(
    PREST_RESPONSE*                  ppResponse,
    char*                            version
    )
{
    uint32_t                         dwError = REST_ENGINE_SUCCESS;
    uint32_t                         versionLen = 0;
    PREST_RESPONSE                   pResponse = NULL;


    if (ppResponse == NULL || *ppResponse == NULL || version == NULL)
    {
        VMREST_LOG_DEBUG("VmRESTSetHttpStatusVersion(): Invalid params");
        dwError = VMREST_HTTP_INVALID_PARAMS;
    }
    BAIL_ON_VMREST_ERROR(dwError);

    pResponse = *ppResponse;

    versionLen = strlen(version);

    if (versionLen > MAX_VERSION_LEN)
    {
        VMREST_LOG_DEBUG("VmRESTSetHttpStatusVersion(): Bad version length");
        dwError = VMREST_HTTP_VALIDATION_FAILED;
    }
    BAIL_ON_VMREST_ERROR(dwError);

    strcpy(pResponse->statusLine->version, version);

cleanup:
    return dwError;
error:
    goto cleanup;
}

uint32_t
VmRESTSetHttpReasonPhrase(
    PREST_RESPONSE*                  ppResponse,
    char*                            reasonPhrase
    )
{
    uint32_t                         dwError = REST_ENGINE_SUCCESS;
    PREST_RESPONSE                   pResponse = NULL;


    if (ppResponse == NULL || *ppResponse == NULL || reasonPhrase == NULL)
    {
        VMREST_LOG_DEBUG("VmRESTSetHttpReasonPhrase(): Invalid params");
        dwError = VMREST_HTTP_INVALID_PARAMS;
    }
    BAIL_ON_VMREST_ERROR(dwError);

    pResponse = *ppResponse;

    strcpy(pResponse->statusLine->reason_phrase, reasonPhrase);

cleanup:
    return dwError;
error:
    goto cleanup;
}

