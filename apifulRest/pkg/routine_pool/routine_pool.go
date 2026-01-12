package routinepool

type Routinepool interface {
}

type routinepool struct {
	
}

func New(poolCount int) Routinepool {
	return &routinepool{}
}
