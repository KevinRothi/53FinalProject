src = $(wildcard *.c)
obj = $(src:.c=.o)

output: $(obj)
	$(CC) -o $@ $^

.PHONY: clean
clean:
	rm -f $(obj) output
