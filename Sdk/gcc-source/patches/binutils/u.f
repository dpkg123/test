
	character *(30)x
	x = 'This is	a    test'
	call tt(x)
	end

	subroutine tt(x)
	character *(30)x
	character *(1)c

	c = '	'

	i = index(x,c)

	write(6,*) x, i
1	format(" ")
	write(6,*) x(i:)
	end

