#include "RedisProxyConn.h"

#include <hiredis-vip/hircluster.h>
#include "base/BaseUtil.h"

#define REDIS_CONNECT_TIMEOUT 200000

RedisProxyConn::RedisProxyConn(const char * addrs):
    addrs_(addrs),
    context_(nullptr)
{
    init();
}

RedisProxyConn::~RedisProxyConn()
{
    release();
}

bool RedisProxyConn::init()
{
    if(context_ != nullptr)
    {
        return true;
    }

    struct timeval timeout = {0, REDIS_CONNECT_TIMEOUT};
	context_ = redisClusterConnectWithTimeout(addrs_.c_str(), timeout, HIRCLUSTER_FLAG_NULL);
    if(!context_ || context_->err)
    {
		if(context_)
		{
			release();
		}
		else
		{
			LOG_INFO("redisConnect failed!!!");
		}

		return false;
    }

    LOG_INFO("connect redis success!!!");
    return true;
}

void RedisProxyConn::release()
{
    if(context_)
    {
        if(context_->err)
        {
            LOG_DEBUG("redisCommand failed:%s", context_->errstr);
        }
        redisClusterFree(context_);
        context_ = nullptr;
    }
}

bool RedisProxyConn::command(const char * format, ...)
{
	if(!init())
    {
        return false;
    }

    std::string strCmd;
    va_list arglist;
    va_start(arglist, format);
    base::vsprintfex(strCmd, format, arglist);
    va_end(arglist);

    LOG_DEBUG("strCmd:%s", strCmd.c_str());

    bool bValue = false;
	redisReply * reply = (redisReply *)redisClusterCommand(context_, strCmd.c_str());
	if(!reply)
    {
        release();
    }
    else
    {
        freeReplyObject(reply);
        bValue = true;
    }
	return bValue;
}

bool RedisProxyConn::vcommand(const char * format, ...)
{
    if(!init())
    {
        return false;
    }

    {
        std::string strCmd;
        va_list arglist;
        va_start(arglist, format);
        base::vsprintfex(strCmd, format, arglist);
        va_end(arglist);

        LOG_DEBUG("strCmd:%s", strCmd.c_str());
    }

    va_list arglist;
    va_start(arglist, format);
    bool bValue = false;
	redisReply * reply = (redisReply *)redisClustervCommand(context_, format, arglist);
	if(!reply)
    {
        release();
    }
    else
    {
        freeReplyObject(reply);
        bValue = true;
    }

    va_end(arglist);
	return bValue;
}

redisReply * RedisProxyConn::_vcommand(const char * format, ...)
{
    if(!init())
    {
        return nullptr;
    }

    {
        std::string strCmd;
        va_list arglist;
        va_start(arglist, format);
        base::vsprintfex(strCmd, format, arglist);
        va_end(arglist);

        LOG_DEBUG("strCmd:%s", strCmd.c_str());
    }

    va_list arglist;
    va_start(arglist, format);
	redisReply * reply = (redisReply *)redisClustervCommand(context_, format, arglist);
    va_end(arglist);

	return reply;
}

bool RedisProxyConn::exists(const char * key)
{
    if(!init())
    {
        return false;
    }

    redisReply * reply = (redisReply*) redisClusterCommand(context_, "EXISTS %s", key);
    if(!reply)
    {
        release();
        return false;
    }
    long ret_value = reply->integer;
    freeReplyObject(reply);

    return 0 == ret_value? false: true;
}

std::string RedisProxyConn::get(const char * key)
{
    std::string ret_value;

    redisReply * reply = _vcommand("GET %s", key);
	if(!reply)
    {
		release();
		return ret_value;
	}

	if(reply->type == REDIS_REPLY_STRING)
    {
		ret_value.append(reply->str, reply->len);
	}

	freeReplyObject(reply);
	return ret_value;
}

bool RedisProxyConn::mget(const KeyList & keys, ValueMap & retValue)
{
    assert(!keys.empty());
    if(!init())
    {
        return false;
    }

    std::string strCmd = "MGET";
    for(KeyList::const_iterator it = keys.begin(); it != keys.end(); ++it)
    {
        strCmd += " " + *it;
    }

    redisReply * reply = (redisReply*) redisClusterCommand(context_, strCmd.c_str());
    if(!reply)
    {
        release();
        return false;
    }

    assert(reply->type == REDIS_REPLY_ARRAY);

    for(size_t i=0; i<reply->elements; ++i)
    {
        redisReply * child_reply = reply->element[i];
        if(child_reply->type == REDIS_REPLY_STRING)
        {
            retValue[keys[i]] = child_reply->str;
        }
    }

    freeReplyObject(reply);
    return true;
}

bool RedisProxyConn::hexists(const char * key, const char * item)
{
    redisReply * reply = _vcommand("HEXISTS %s %s", key, item);
    if(!reply)
    {
        release();
        return false;
    }
    long ret_value = reply->integer;
    freeReplyObject(reply);

    return 0 == ret_value? false: true;
}

bool RedisProxyConn::hdel(const char * key, const ItemList & items)
{
    if(!init())
    {
        return false;
    }

    int argc = items.size() + 2;
    const char ** argv = new const char *[argc];
    if(!argv)
    {
        return false;
    }

    argv[0] = "HDEL";
    argv[1] = key;
    int index = 2;
    for(size_t i = 0; i < items.size(); ++i)
    {
        argv[index++] = items[i].c_str();
    }

	redisReply* reply = (redisReply *)redisClusterCommandArgv(context_, argc, argv, NULL);
	if (!reply)
    {
		delete []argv;
		release();
        return false;
	}

	delete []argv;

	long ret_value = reply->integer;
    freeReplyObject(reply);

    return 0 == ret_value? false: true;
}

bool RedisProxyConn::hmset(const char * key, const ItemList & items, const char * value)
{
    if(!init())
    {
        return false;
    }

    int argc = items.size()*2 + 2;
    const char ** argv = new const char *[argc];
    if(!argv)
    {
        return false;
    }

    argv[0] = "HMSET";
    argv[1] = key;
    int index = 2;
    for(size_t i = 0; i < items.size(); ++i)
    {
        argv[index++] = items[i].c_str();
        argv[index++] = value;
    }

	redisReply* reply = (redisReply *)redisClusterCommandArgv(context_, argc, argv, NULL);
	if (!reply)
    {
		delete []argv;
		release();
        return false;
	}

	delete []argv;

	long ret_value = reply->integer;
    freeReplyObject(reply);

    return 0 == ret_value? false: true;
}

bool RedisProxyConn::hmset(const char * key, const ValueMap & valueMap)
{
    if(!init())
    {
        return false;
    }

    int argc = valueMap.size()*2 + 2;
    const char ** argv = new const char *[argc];
    if(!argv)
    {
        return false;
    }

    argv[0] = "HMSET";
    argv[1] = key;
    int index = 2;
    for(auto it = valueMap.begin(); it != valueMap.end(); ++it)
    {
        argv[index++] = it->first.c_str();
        argv[index++] = it->second.c_str();
    }

	redisReply* reply = (redisReply *)redisClusterCommandArgv(context_, argc, argv, NULL);
	if (!reply)
    {
		delete []argv;
		release();
        return false;
	}

	delete []argv;

	long ret_value = reply->integer;
    freeReplyObject(reply);

    return 0 == ret_value? false: true;
}

std::string RedisProxyConn::hget(const char * key, const char * item)
{
    std::string ret_value;

    redisReply * reply = _vcommand("HGET %s %s", key, item);
	if(!reply)
    {
		release();
		return ret_value;
	}

	if(reply->type == REDIS_REPLY_STRING)
    {
		ret_value.append(reply->str, reply->len);
	}

	freeReplyObject(reply);
	return ret_value;
}

bool RedisProxyConn::hmget(const char * key, const ItemList & items, ValueMap & retValue)
{
    assert(key && !items.empty());

    if(!init())
    {
        return false;
    }

    std::string strCmd = "HMGET ";
    strCmd += key;
    for(ItemList::const_iterator it = items.begin(); it != items.end(); ++it)
    {
        strCmd += " " + *it;
    }

    LOG_DEBUG("szCmd:%s", strCmd.c_str());
    redisReply * reply = (redisReply*) redisClusterCommand(context_, strCmd.c_str());
    if(!reply)
    {
        release();
        return false;
    }

    //assert(reply->type == REDIS_REPLY_ARRAY);
    for(size_t i = 0; i < reply->elements; ++i)
    {
        redisReply* child_reply = reply->element[i];
        if(child_reply->type == REDIS_REPLY_STRING)
        {
            retValue[items[i]] = child_reply->str;
        }
    }

    freeReplyObject(reply);
    return true;
}

bool RedisProxyConn::hgetall(const char * key, ValueMap & retValue)
{
    bool bValue = false;
	redisReply * reply = _vcommand("HGETALL %s", key);
	if(!reply)
	{
		release();
	}
    else
    {
        if((reply->type == REDIS_REPLY_ARRAY) && (reply->elements % 2 == 0))
        {
            for(size_t i = 0; i < reply->elements; i += 2)
            {
                redisReply * field_reply = reply->element[i];
                redisReply * value_reply = reply->element[i + 1];

                std::string field(field_reply->str, field_reply->len);
                std::string value(value_reply->str, value_reply->len);
                retValue.insert(make_pair(field, value));
            }
        }

        freeReplyObject(reply);
        bValue = true;
    }

	return bValue;
}

bool RedisProxyConn::hgetall(const char * key, LONGValueMap & retValue)
{
    bool bValue = false;
	redisReply * reply = _vcommand("HGETALL %s", key);
	if(!reply)
	{
		release();
	}
    else
    {
        if((reply->type == REDIS_REPLY_ARRAY) && (reply->elements % 2 == 0))
        {
            for(size_t i = 0; i < reply->elements; i += 2)
            {
                redisReply * field_reply = reply->element[i];
                redisReply * value_reply = reply->element[i + 1];

                retValue.insert(std::make_pair(atoll(field_reply->str), atoll(value_reply->str)));
            }
        }

        freeReplyObject(reply);
        bValue = true;
    }

	return bValue;
}

bool RedisProxyConn::hgetall(const char * key, LONGValueList & retValue, bool bKey)
{
    bool bValue = false;
	redisReply * reply = _vcommand("HGETALL %s", key);
	if(!reply)
	{
		release();
	}
    else
    {
        if((reply->type == REDIS_REPLY_ARRAY) && (reply->elements % 2 == 0))
        {
            int nPos = bKey? 0: 1;
            for(size_t i = 0; i < reply->elements; i += 2)
            {
                redisReply * element_reply = reply->element[i+nPos];
                retValue.emplace_back(atoll(element_reply->str));
            }
        }

        freeReplyObject(reply);
        bValue = true;
    }

	return bValue;
}

bool RedisProxyConn::smembers(const char * key, ValueList & retValue)
{
    redisReply * reply = _vcommand("SMEMBERS %s", key);
    if(!reply)
    {
        release();
        return false;
    }

    for(size_t i = 0; i < reply->elements; ++i)
    {
        redisReply* child_reply = reply->element[i];
        if(child_reply->type == REDIS_REPLY_STRING)
        {
            retValue.emplace_back(child_reply->str);
        }
    }

    freeReplyObject(reply);
    return true;
}

bool RedisProxyConn::smembers(const char * key, LONGValueList & retValue)
{
    redisReply * reply = _vcommand("SMEMBERS %s", key);
    if(!reply)
    {
        release();
        return false;
    }

    for(size_t i = 0; i < reply->elements; ++i)
    {
        redisReply* child_reply = reply->element[i];
        if(child_reply->type == REDIS_REPLY_STRING)
        {
            retValue.emplace_back(atol(child_reply->str));
        }
    }

    freeReplyObject(reply);
    return true;
}

bool RedisProxyConn::sismember(const char * key, long item)
{
    redisReply * reply = _vcommand("SISMEMBER %s %lu", key, item);
    if(!reply)
    {
        release();
        return false;
    }

    long ret_value = reply->integer;
    freeReplyObject(reply);

    return 0 == ret_value? false: true;
}

long RedisProxyConn::scard(const char * key)
{
    redisReply * reply = _vcommand("SCARD %s", key);
    if(!reply)
    {
        release();
        return 0;
    }

    long ret_value = reply->integer;
    freeReplyObject(reply);

    return ret_value;
}

long RedisProxyConn::incr(const char * key)
{
    redisReply * reply = _vcommand("INCR %s", key);
    if(!reply)
    {
        release();
        return -1;
    }

    long ret_value = reply->integer;

    freeReplyObject(reply);
    return ret_value;
}

long RedisProxyConn::incrby(const char * key, long value)
{
    redisReply * reply = _vcommand("INCRBY %s %lu", key, value);
    if(!reply)
    {
        release();
        return -1;
    }

    long ret_value = reply->integer;

    freeReplyObject(reply);
    return ret_value;
}

long RedisProxyConn::hincrby(const char * key, const char * item, long value)
{
    redisReply * reply = _vcommand("HINCRBY %s %s %lu", key, item, value);
    if(!reply)
    {
        release();
        return -1;
    }

    long ret_value = reply->integer;

    freeReplyObject(reply);
    return ret_value;
}

bool RedisProxyConn::expire_day(const char * key, int days)
{
    redisReply * reply = _vcommand("EXPIRE %s %lu", key, 86400*days);
    if(!reply)
    {
        release();
        return false;
    }

    long ret_value = reply->integer;

    freeReplyObject(reply);
    return ret_value;
}

bool RedisProxyConn::persist(const char * key)
{
    redisReply * reply = _vcommand("PERSIST %s", key);
    if(!reply)
    {
        release();
        return false;
    }

    long ret_value = reply->integer;

    freeReplyObject(reply);
    return ret_value;
}
