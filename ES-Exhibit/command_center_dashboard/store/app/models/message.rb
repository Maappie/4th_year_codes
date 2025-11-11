class Message < ApplicationRecord
  validates :sender_tag, presence: true, length: { maximum: 64 }
  validates :message,    presence: true, length: { maximum: 2048 }
  validates :nonce,      presence: true, format: { with: /\A\h{24}\z/ }

  after_create_commit  -> { broadcast_prepend_later_to :messages }
  after_update_commit  -> { broadcast_replace_later_to :messages }  
  after_destroy_commit -> { broadcast_remove_to        :messages }  
end