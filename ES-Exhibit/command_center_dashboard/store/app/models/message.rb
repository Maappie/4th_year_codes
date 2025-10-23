class Message < ApplicationRecord
  validates :sender_tag, presence: true, length: { maximum: 64 }
  validates :message,    presence: true, length: { maximum: 2048 }
  validates :nonce,      presence: true, format: { with: /\A\h{24}\z/ } # 12-byte hex
end
