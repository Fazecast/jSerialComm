clean:
	$(MAKE) -C toolchain clean
	./mvnw clean
	@rm dependency-reduced-pom.xml

native:
	$(MAKE) -C toolchain make

build: native
	./mvnw clean package

test: build
	java -jar target/jSerialComm-$(shell ls target/*-test.jar | cut -d '-' -f 2)-test.jar
