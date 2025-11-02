clean:
	$(MAKE) -C toolchain clean
	./mvnw clean
	@rm -f dependency-reduced-pom.xml

bump:
	$(MAKE) -C toolchain bump

native:
	$(MAKE) -C toolchain make

build: native
	./mvnw clean package -Ptesting

test: build
	java -jar target/jSerialComm-$(shell ls target/*-test.jar | cut -d '-' -f 2)-test.jar
