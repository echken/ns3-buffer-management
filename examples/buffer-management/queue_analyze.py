import sys

def main(argv):

    print "Reading ..."

    sum_value = 0
    length = 0
    value_list = []

    with open (argv[1]) as f:
        read_data = f.readlines()[4:-1]
    f.closed
    for line in read_data:
        splited_data = line.split (' ', 1)
        value = int (splited_data[1])
        sum_value += value
        length += 1
        value_list.append(value)

    value_list.sort ()

    print "The average queue length: %f" % (sum_value / length)
    print "The 99 queue length: %f" % (value_list[int(length * 0.99)])

if __name__ == '__main__':
    main (sys.argv)
