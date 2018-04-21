import argparse
import re
import cmd2
def test1():
    parser = argparse.ArgumentParser();
    parser.parse_args()
    a = 1
    def func():
        print "HelloWorld"
    #print "HelloPiKaQ"
    #print func()
    def xxCmd(func):
        print('%s ' % (func.__name__))
        def _XX(parm):
            print('%s ' %(func.__name__))
            print parm
            print a
            return func(parm)
        return _XX


    @xxCmd
    def hello(parm):
        pass
    hello("world")
def test2():
    a = "tables -m   {x x}   -a { X  X} add"
    b = a.split(' ')

    c = re.split(r'\s+',a)
    print c
#    d = re.split(r'')
    a = "  {  }  {  }  "
    e = re.split(r'(\s+)',a)
    print e

    x = "tables add   -t fwd_table   -r test -m {   }"
    y = re.split(r'^(tables|version|meters|counts)\s+(add|list|list-rules|delete|edit)\s+|(-[a-z])',x)
    def notEmpty(s):
        return s and s.strip()
    z = ['A','',None]
    filter(notEmpty,z)
    print z
    y=filter(notEmpty,y)
    print y

    def not_empty(s):
        return s and s.strip()

    z = ['A', '', 'B', None, 'C', '  ']
    z = filter(not_empty,z )
    print z
#    print c
 #   print b
def test2_1():
    if re.match(r'[a-zA-Z_.]*@[a-zA-Z*_.]*(.com)',"bill.gates@example.cn"):
        print 'true'
        return True
    else:
        print 'false'
        return False
def test3():
    parser = argparse.ArgumentParser(description = "Test")
    parser.add_argument('integers',type=int , nargs='+',help="cc")
    parser.add_argument('--sum', dest='accumulate', action='store_const',
    const = sum, default = max,
    help = 'sum the integers (default: find the max)')
    print parser.parse_args()
    pass
def test4():
    a = None
    try :
        pass
    except :
        KeyboardInterrupt
    if not a :raise Exception, "caonibaba"
def test5():
    def test(args):
        print args
        if not args.index:
            print "hello"
        else:
            print "world"

    parser = argparse.ArgumentParser(description = "Test")
    parser.add_argument('-i', '--index', dest='index', default='', type=str,
                                  help="index to start from, default 0")
    parser.add_argument('-c', '--count', dest='count', default=-1, type=int,
                                  help="number of entries to read, default -1 (all)")
    parser.set_defaults(func=test)
    args=parser.parse_args()
    args.func(args)

    # test(args)






if __name__ == '__main__':
    test5()
