import simplejson
import sys
import os
from collections import OrderedDict
# class JSONObject:
#     def __init__(self,d):
#         self.__dict__ = d

fp = open('/home/niubin/POFSwitch-RoleSwitch/pof.json','r')
data = fp.read()
print data

b = eval(data)
print b
print type(b)
j = ((simplejson.dumps(b)))
print('j = %s\n' %j)
load_dict = {}


x = {"fwd_table":{"match":{"ipv4.srcAddr":{"value":"10.0.0.1/20"},"ipv4.dstAddr":{"value":"10.0.0.2/8"}},"name":"fwd","action":{"data":{"port":{"vaule":"v0.0"}},"type":"forward_act"}}}
y = simplejson.dumps(x)

print x
print y
try:
    pass
except:
    print('bad json %s' % x)
load_dict = simplejson.loads(j)
#load_dict = json.load(fp)
print ("fp = %s\n" % load_dict)
print type(load_dict)


a = {"key":{"subkey":"value"}}
json_str = simplejson.dumps(a)
print (json_str)
b = simplejson.loads(json_str)
print b["key"]
fp.close()
s = """{'ipv4.srcAddr': {'value': '10.0.0.1/20'}, 'ipv4.dstAddr': {'value': '10.0.0.2/8'}}"""
print s
t = ''
for i in range(len(s)):
    if s[i] == "\'" :
        t = s[:i] + '\'' + s[:i]
print t
