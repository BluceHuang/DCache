/**
* Tencent is pleased to support the open source community by making DCache available.
* Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
* Licensed under the BSD 3-Clause License (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of the License at
*
* https://opensource.org/licenses/BSD-3-Clause
*
* Unless required by applicable law or agreed to in writing, software distributed under
* the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
* either express or implied. See the License for the specific language governing permissions
* and limitations under the License.
*/
#include "StringUtil.h"
#include <string.h>

const size_t StringUtil::GZIP_MIN_STR_LEN = 9 * 1024 * 1024;

bool StringUtil::gzipCompress(const char* src, size_t length, string& buffer)
{
    buffer.clear();

    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK)
    {
        return false;
    }

    static char gz_simple_header[] = { '\037', '\213', '\010', '\000', '\000', '\000', '\000', '\000', '\002', '\377' };

    size_t destLen = sizeof(gz_simple_header) + length * 2;
    char *out = new char[destLen];

    stream.next_out = (Bytef *)out;
    stream.avail_out = destLen;

    stream.next_in = (Bytef *)src;
    stream.avail_in = length;

    memcpy(stream.next_out, gz_simple_header, sizeof(gz_simple_header));
    stream.next_out += sizeof(gz_simple_header);
    stream.avail_out -= sizeof(gz_simple_header);

    int ret = deflate(&stream, Z_FINISH);
    if (ret != Z_STREAM_END)
    {
        delete[] out;
        return false;
    }

    destLen = destLen - stream.avail_out;

    uLong  crc = crc32(0, Z_NULL, 0);

    crc = crc32(crc, (const Bytef*)src, length);

    memcpy(out + destLen, &crc, 4);

    memcpy(out + destLen + 4, &length, 4);

    destLen += 8;

    buffer.assign(out, destLen);

    delete[] out;

    deflateEnd(&stream);

    return true;
}

bool StringUtil::gzipUncompress(const char* src, size_t length, string& buffer)
{
    buffer.clear();

    z_stream strm;

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;

    int ret = inflateInit2(&strm, 47);

    if (ret != Z_OK)
        return false;

    strm.avail_in = length;
    strm.next_in = (unsigned char*)src;

    static size_t CHUNK = 1024 * 256;
    unsigned char *out = new unsigned char[CHUNK];

    /* run inflate() on input until output buffer not full */
    do
    {
        strm.avail_out = CHUNK;
        strm.next_out = out;

        ret = inflate(&strm, Z_NO_FLUSH);

        assert(ret != Z_STREAM_ERROR); /* state not clobbered */
        switch (ret)
        {
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR;	/* and fall through */
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            inflateEnd(&strm);
            delete[] out;
            return false;
        }
        buffer.append((char*)out, CHUNK - strm.avail_out);

    } while (strm.avail_out == 0);

    /* clean up and return */
    inflateEnd(&strm);
    delete[] out;

    return(ret == Z_STREAM_END);
}

bool StringUtil::parseString(const string& str, vector<string>& res)
{
    const char* p = str.c_str();
    size_t iLen = 0, pos = 0, iTotalLen = str.length();
    static size_t iTypeLen = sizeof(uint32_t);
    while (pos < iTotalLen)
    {
        if (pos + iTypeLen >= iTotalLen)
        {
            return false;
        }

        memcpy(&iLen, p + pos, iTypeLen);
        iLen = ntohl(iLen);
        pos += iTypeLen;
        if (pos + iLen > iTotalLen)
        {
            return false;
        }
        else
        {
            res.push_back(string(p + pos, iLen));
            pos += iLen;
        }
    }

    return true;
}


