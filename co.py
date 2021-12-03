import gdb, traceback;

# https://sourceware.org/gdb/current/onlinedocs/gdb/Python-API.html#Python-API

def gdb_value_eval(val, cmd):
    if val.type.code == gdb.TYPE_CODE_STRUCT:
        val = val.address
    if val.type.code != gdb.TYPE_CODE_PTR:
        raise Exception("gdb_value_eval input type error")
    cmd = "(*(\'%s\'*)0x%x).%s" % (val.type.target(), int(val), cmd)
    return gdb.parse_and_eval(cmd)

def gdb_value_eval_type(val, vtype, cmd):
    if val.type.code != gdb.TYPE_CODE_PTR:
        raise Exception("gdb_value_eval input type error")
    cmd = "(*(\'%s\'*)0x%x).%s" % (vtype, int(val), cmd)
    return gdb.parse_and_eval(cmd)

def print_coroutine(pco):
    sfunc = str(gdb_value_eval(pco, "pfn"))
    parg = gdb_value_eval(pco, "arg")
    print("0x%x\t%s\t0x%x" % (int(pco), sfunc, int(parg)))

'''
Usage:
    show_coroutine
'''
class ShowCoroutine(gdb.Command):
    def __init__(self):
        super(ShowCoroutine, self).__init__('show_coroutine', gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        colist = gdb.parse_and_eval("gCoEnvPerThread.activeRoutine")
        print("co\tfunc\targ")

        count = 0
        pco = gdb_value_eval(colist, "head")
        while int(pco) != 0:
            print_coroutine(pco)
            count = count + 1
            pco = gdb_value_eval(pco, "pNext")
        print("coutinue size: %d" % (count,))

ShowCoroutine()

'''
Usage:
    costack co_addr
'''
class CoStack(gdb.Command):
    def __init__(self):
        super(CoStack, self).__init__('costack', gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        pco = int(arg, base=16)
        rbp = gdb.parse_and_eval("((stCoRoutine_t*)(0x%x))->ctx->regs[6]" % (pco,)).__int__()
        print("stCoRoutine_t 0x%x, store rbp 0x%x" % (pco, rbp))
        try:
            while rbp > 0:
                resp = gdb.execute('x/2xa %s'%(rbp), to_string=True).split('\t')
                rbp = int(resp[1], base=16)
                crip = resp[2].strip()
                print("0x%x: %s" % (rbp, crip))
        except:
            traceback.print_exc()

CoStack()

'''
Usage:
    coparent
'''
class CoParent(gdb.Command):
    def __init__(self):
        super(CoParent, self).__init__('coparent', gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        stsize = int(gdb.parse_and_eval("gCoEnvPerThread.iCallStackSize"))
        print("stack size %d" % (stsize,))
        print("co\tfunc\targ")
        for idx in range(stsize):
            pco = gdb.parse_and_eval("gCoEnvPerThread.pCallStack[%d]" % (idx,))
            print_coroutine(pco)

CoParent()