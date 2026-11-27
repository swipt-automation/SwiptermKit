package libswipterm

import (
	"github.com/swipt-automation/SwiptermKit/internal/netstack"
)

type Network struct {
	stack *netstack.Stack
}

func CreateNetwork() (*Network, error) {
	stack, err := netstack.New()
	if err != nil {
		return nil, err
	}
	return &Network{stack: stack}, nil
}

func (n *Network) Handle() int64 {
	return int64(n.stack.Handle())
}

func (n *Network) ResetNetwork() {
	n.stack.ResetNetwork()
}

func (n *Network) Close() error {
	return n.stack.Close()
}
