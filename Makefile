INC=-I/usr/local/include

CC=clang
# Find all C source files in the directory
SOURCES = $(wildcard tests/*.c)
SOURCES += $(wildcard benchmarks/*.c)

# Convert the .c files to .o files
OBJECTS = $(SOURCES:.c=.o)
BCOBJS = $(SOURCES:.c=.bc)
M2ROBJS = $(BCOBJS:.bc=-m2r.ll)
OPTOBJS = $(M2ROBJS:-m2r.ll=-m2r-opt.ll)
EXECOBJS = $(OPTOBJS:-m2r-opt.ll=-opt.o)

all: ConstantFolder.so $(OBJECTS) $(BCOBJS) $(M2ROBJS) $(OPTOBJS) $(EXECOBJS)

CXXFLAGS = -rdynamic $(shell llvm-config --cxxflags) $(INC) -fPIC -g -O0

# Bytecode generation
%.bc: %.c
	clang -fno-discard-value-names -Xclang -disable-O0-optnone -O0 -emit-llvm -c $< -o $@

# Unoptimised mem2reg mapped IR generation
%-m2r.ll: %.bc
	opt -S -bugpoint-enable-legacy-pm=1 -mem2reg  $< -o $@

# Optimised IR after running it through pass
%-m2r-opt.ll: %-m2r.ll
	opt -S -bugpoint-enable-legacy-pm=1 -load ./ConstantFolder.so -constfold $< -o $@

# Executable from unoptimised LLVM
%.o: %-m2r.ll
	clang $< -o $@

# Executable from optimised LLVM
%-opt.o: %-m2r-opt.ll
	clang $< -o $@

# Generating the pass
%.so: %.o
	$(CXX) -dlyb -shared $^ -o $@

# To print stats with lli
print-stats:
	@for file in tests/*m2r*.ll; do \
		count=$$(lli -stats -force-interpreter "$$file" 2>&1 | grep "interpreter - Number of dynamic instructions executed" | grep -o '[0-9]\+'); \
		echo "$$file: $$count"; \
	done

# To print stats with lli
print-stats-benchy:
	@for file in benchmarks/*m2r*.ll; do \
		count=$$(lli -stats -force-interpreter "$$file" 2>&1 | grep "interpreter - Number of dynamic instructions executed" | grep -o '[0-9]\+'); \
		echo "$$file: $$count"; \
	done

clean: 
	rm -fr *.o *.so tests/*.ll tests/*.bc tests/*.o
