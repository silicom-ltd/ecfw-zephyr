import argparse
import math

def rvalfromtemp(rnom, tnom, bval, temp):
    exp = (1 / temp) - (1 / tnom)
    exp *= bval
    return (rnom * (math.e ** exp))

def rvaltoadc(rval, pup, res):
    return ((res-1) * rval) / (pup + rval)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Generate LUT for ADC values.')
    parser.add_argument('--rnominal', help='Rnominal value for NTC thermistor', type=int)
    parser.add_argument('--tnominal', help='Tnominal value for NTC thermistor', type=int)
    parser.add_argument('--bconst', help='B constant value for NTC thermistor', type=int)
    parser.add_argument('--rpullup', help='R value for pullup resistor for this thermistor', type=int)
    parser.add_argument('--res', help='Resolution (in bits) for the LUT', type=int)
    parser.add_argument('--tstart', help='Starting temperature', type=int)
    parser.add_argument('--tstop', help='Ending temperature', type=int)

    args = parser.parse_args()

    rnominal = args.rnominal
    tnominal = args.tnominal
    bconst = args.bconst
    rpullup = args.rpullup
    res = args.res
    tstart = args.tstart
    tstop = args.tstop
    output = open("therm.h", "w")
    output.write("/* This table is automatically generated! DO NOT EDIT, see generate_tbl.py. */\n")
    output.write("/* %d-step therm table: bValue = %s */ \n\n" % (2**res, bconst))
    output.write("const uint16_t therm_table[%d] = {\n" % (2**res))
    for val in range(tstart, tstop+1):
        if val == tnominal:
            r = rnominal
        else:
            r = rvalfromtemp(rnominal, tnominal+273, bconst, val+273)
        output.write("\t %d, \n" % rvaltoadc(int(r), rpullup, 2**res))
    output.write("};\n")
    output.write("#define TSTART %d\n" % tstart)
    output.write("#define TSTOP %d\n" % tstop)
    output.close
